/*
 * qosMngr.c
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

/** \file qosMngr.c
 *  \brief QOS module interface
 *
 *  \see qosMngr.h
 */

/****************************************************************************************************/
/*																									*/
/*		MODULE:		qosMGr.c																	    */
/*		PURPOSE:	QOS module interface.												            */
/*                  This module handles the QOS manager configuration.	 							*/
/*																						 			*/
/****************************************************************************************************/
#define __FILE_ID__  FILE_ID_74
#include "report.h"
#include "osApi.h"
#include "paramOut.h"
#include "siteMgrApi.h"
#include "qosMngr.h"
#include "qosMngr_API.h"
#include "sme.h"
#include "EvHandler.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#include "XCCTSMngr.h"
#endif
#include "TWDriver.h"
#include "DrvMainModules.h"
#include "StaCap.h"
#include "roamingMngrApi.h"


extern int WMEQosTagToACTable[MAX_NUM_OF_802_1d_TAGS];

/* Translate input AC to TID */            
const TI_UINT8 WMEQosAcToTid[MAX_NUM_OF_AC] = { 0, 2, 4, 6 };

/* Translate input TID to the other TID of the same AC */
const TI_UINT32 WMEQosMateTid[MAX_NUM_OF_802_1d_TAGS] = { 3, 2, 1, 0, 5, 4, 7, 6 };

/* Used to indicate no user priority is assigned for AC */
#define INACTIVE_USER_PRIORITY 0xFF

/* Used for TSPEC nominal fixed size */
#define FIXED_NOMINAL_MSDU_SIZE_MASK 0x8000


/********************************************************************************/
/*						Internal functions prototypes.							*/
/********************************************************************************/
static void release_module(qosMngr_t *pQosMngr, TI_UINT32 initVec);
static TI_STATUS verifyAndConfigTrafficParams(qosMngr_t *pQosMngr, TQueueTrafficParams *pQtrafficParams);
static TI_STATUS verifyAndConfigQosParams(qosMngr_t *pQosMngr,TAcQosParams *pAcQosParams);
static TI_STATUS getWMEInfoElement(qosMngr_t *pQosMngr,TI_UINT8 *pWMEie,TI_UINT8 *pLen);
static TI_STATUS verifyWmeIeParams(qosMngr_t *pQosMngr,TI_UINT8 *pQosIeParams);
static TI_STATUS updateACParams(qosMngr_t *pQosMngr,dot11_ACParameters_t *pAcParams);
static TI_STATUS setWmeSiteParams(qosMngr_t *pQosMngr, TI_UINT8 *pQosIeParams);
static void qosMngr_resetAdmCtrlParameters(TI_HANDLE hQosMngr);
static TI_STATUS qosMngr_getCurrAcStatus(TI_HANDLE hQosMngr, OS_802_11_AC_UPSD_STATUS_PARAMS *pAcStatusParams);
static void deleteTspecConfiguration(qosMngr_t *pQosMngr, TI_UINT8 acID);
static void setNonQosAdmissionState(qosMngr_t *pQosMngr, TI_UINT8 acID);
static void qosMngr_storeTspecCandidateParams (tspecInfo_t *pCandidateParams, OS_802_11_QOS_TSPEC_PARAMS *pTSPECParams, TI_UINT8 ac);
static TI_STATUS qosMngr_SetPsRxStreaming (qosMngr_t *pQosMngr, TPsRxStreaming *pNewParams);

/********************************************************************************
 *							qosMngr_create										*
 ********************************************************************************
DESCRIPTION: QOS module creation function, called by the config mgr in creation phase. 
				performs the following:
				- Allocate the QOS MNGR handle.				                                                                                                   
INPUT:      hOs -			Handle to OS		


OUTPUT:		

RETURN:     Handle to the QOS MNGR module on success, NULL otherwise

************************************************************************/
TI_HANDLE qosMngr_create(TI_HANDLE hOs)
{
	qosMngr_t		*pQosMngr = NULL;
	TI_UINT32			initVec = 0;

	if(!hOs)
		return NULL;
	
	/* allocating the WME object */
	pQosMngr = os_memoryAlloc(hOs,sizeof(qosMngr_t));

	if (pQosMngr == NULL)
    {
        return NULL;
    }

    os_memoryZero (hOs, pQosMngr, sizeof(qosMngr_t));

	initVec |= (1 << QOS_MNGR_INIT_BIT_LOCAL_VECTOR);
    
	/* create admission control object */
	pQosMngr->pTrafficAdmCtrl = trafficAdmCtrl_create(hOs);

	if (pQosMngr->pTrafficAdmCtrl == NULL)
	{
		qosMngr_destroy(pQosMngr);
		return NULL;
	}

	initVec |= (1 << QOS_MNGR_INIT_BIT_ADM_CTRL);

	return(pQosMngr);
}

/************************************************************************
 *                        qosMngr_destroy							    *
 ************************************************************************
DESCRIPTION: QOS MNGR module destroy function, called by the config mgr in
				 the destroy phase 
				 performs the following:
				-	Free all memory alocated by the module
				
INPUT:      hQosMngr	-	QOS Manager handle.		

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_destroy(TI_HANDLE hQosMngr)
{
	TI_UINT32				   initVec;
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	if (pQosMngr == NULL)
		return TI_OK;

	initVec = 0xFFFF; 
    release_module(pQosMngr, initVec);

	return TI_OK;
}

/***********************************************************************
 *                        release_module									
 ***********************************************************************
DESCRIPTION:	Called by the destroy function or by the create function (on failure)
				Go over the vector, for each bit that is set, release the corresponding module.
                                                                                                   
INPUT:      pQosMngr  -  QOS Mngr pointer.
			initVec	  -	 Vector that contains a bit set for each module thah had been initiualized

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise
************************************************************************/
static void release_module(qosMngr_t *pQosMngr, TI_UINT32 initVec)
{
	
	if (initVec & (1 << QOS_MNGR_INIT_BIT_ADM_CTRL))
		trafficAdmCtrl_unload(pQosMngr->pTrafficAdmCtrl);

	if (initVec & (1 << QOS_MNGR_INIT_BIT_LOCAL_VECTOR))
		os_memoryFree(pQosMngr->hOs, pQosMngr, sizeof(qosMngr_t));
		
	initVec = 0;
}

/************************************************************************
 *                        qosMngr_init		     						*
 ************************************************************************
DESCRIPTION: QOS Manager module configuration function, called by the config 
				mgr in configuration phase
				performs the following:
				-	Reset & initiailzes local variables
				-	Init the handles to be used by the module
                                                                                                   
INPUT:      pStadHandles  - The driver modules handles		


OUTPUT:		

RETURN:     void
************************************************************************/
void qosMngr_init (TStadHandlesList *pStadHandles)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)(pStadHandles->hQosMngr);

    /* init handles */
    pQosMngr->hOs              = pStadHandles->hOs;
    pQosMngr->hReport          = pStadHandles->hReport;
    pQosMngr->hSiteMgr         = pStadHandles->hSiteMgr;
    pQosMngr->hTWD             = pStadHandles->hTWD;
    pQosMngr->hTxCtrl          = pStadHandles->hTxCtrl;
    pQosMngr->hTxMgmtQ         = pStadHandles->hTxMgmtQ;
    pQosMngr->hMeasurementMngr = pStadHandles->hMeasurementMgr;
    pQosMngr->hSmeSm           = pStadHandles->hSme;
    pQosMngr->hCtrlData        = pStadHandles->hCtrlData;
	pQosMngr->hEvHandler       = pStadHandles->hEvHandler;
	pQosMngr->hXCCMgr          = pStadHandles->hXCCMngr;
	pQosMngr->hTimer           = pStadHandles->hTimer;
    pQosMngr->hStaCap          = pStadHandles->hStaCap;
    pQosMngr->hRoamMng         = pStadHandles->hRoamingMngr;

    pQosMngr->isConnected = TI_FALSE;
}


TI_STATUS qosMngr_SetDefaults (TI_HANDLE hQosMngr, QosMngrInitParams_t *pQosMngrInitParams)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
    TI_UINT8   acID, uTid;
    TI_STATUS  status;

    /* init params */
    pQosMngr->WMEEnable = pQosMngrInitParams->wmeEnable;
    pQosMngr->trafficAdmCtrlEnable = pQosMngrInitParams->trafficAdmCtrlEnable;
    pQosMngr->tagZeroConverHeader = pQosMngrInitParams->qosTagZeroConverHeader;
	pQosMngr->qosPacketBurstEnable = pQosMngrInitParams->PacketBurstEnable;
	pQosMngr->qosPacketBurstTxOpLimit = pQosMngrInitParams->PacketBurstTxOpLimit;
	pQosMngr->desiredPsMode = pQosMngrInitParams->desiredPsMode;
    pQosMngr->bCwFromUserEnable = pQosMngrInitParams->bCwFromUserEnable;
    pQosMngr->uDesireCwMin = pQosMngrInitParams->uDesireCwMin;
    pQosMngr->uDesireCwMax = pQosMngrInitParams->uDesireCwMax;
	pQosMngr->bEnableBurstMode = pQosMngrInitParams->bEnableBurstMode;


    pQosMngr->activeProtocol    = QOS_NONE;
    pQosMngr->WMESiteSupport    = TI_FALSE;

	pQosMngr->desiredMaxSpLen	= pQosMngrInitParams->desiredMaxSpLen;

    pQosMngr->voiceTspecConfigured = TI_FALSE;
	pQosMngr->videoTspecConfigured = TI_FALSE;
    pQosMngr->performTSPECRenegotiation = TI_FALSE;
	pQosMngr->TSPECNegotiationResultCallb = NULL;
	pQosMngr->TSPECNegotiationResultModule = NULL;

    /* No template has been set for UPSD */
    pQosMngr->QosNullDataTemplateUserPriority = 0xFF;
	
	TWD_CfgBurstMode(pQosMngr->hTWD, pQosMngr->bEnableBurstMode);

	/* configure admission control parameters */
	qosMngr_resetAdmCtrlParameters(pQosMngr);

	status = trafficAdmCtrl_config (pQosMngr->pTrafficAdmCtrl,
                                    pQosMngr->hTxMgmtQ,
                                    pQosMngr->hReport,
                                    pQosMngr->hOs,
                                    pQosMngr,
                                    pQosMngr->hCtrlData,
                                    pQosMngr->hXCCMgr,
                                    pQosMngr->hTimer,
                                    pQosMngr->hTWD,
                                    pQosMngr->hTxCtrl,
                                    &pQosMngrInitParams->trafficAdmCtrlInitParams);
	if(status != TI_OK)
		return TI_NOK;

	/* 
	 * configure per AC traffic parameters
	 */
    for(acID = FIRST_AC_INDEX;acID < MAX_NUM_OF_AC; acID++)
	{
		/*
		 * setting ac traffic params for TrafficCategoryCfg (TNET configuration 
		 * The parameters can be changed in run-time, so they are saved in "init params"
		 * for 'disconnecting' .
		 * the parameters being set in setSite; "select" phase.
         */
		pQosMngr->acParams[acID].QtrafficParams.queueID       = acID;
		pQosMngr->acParams[acID].QtrafficParams.channelType   = CHANNEL_TYPE_EDCF;
		pQosMngr->acParams[acID].QtrafficParams.tsid          = acID;
		pQosMngr->acParams[acID].QtrafficParams.dot11EDCATableMSDULifeTime = 0;
		pQosMngr->acParams[acID].QtrafficParams.psScheme      = PS_SCHEME_LEGACY;
		pQosMngr->acParams[acID].QtrafficParams.ackPolicy     = pQosMngrInitParams->acAckPolicy[acID];
		pQosMngr->acParams[acID].QtrafficParams.APSDConf[0]   = 0;
		pQosMngr->acParams[acID].QtrafficParams.APSDConf[1]   = 0;


		/* 
		 * Update the qTrafficInitParams as well 
		 */
		os_memoryCopy(pQosMngr->hOs,
                      &pQosMngr->acParams[acID].QTrafficInitParams,
                      &pQosMngr->acParams[acID].QtrafficParams,
                      sizeof(TQueueTrafficParams));

		/* will be config only after select */
		verifyAndConfigTrafficParams(pQosMngr,&(pQosMngr->acParams[acID].QtrafficParams));
		
		/*
		 * setting ac QoS params for acQosParams (TNET configuration) 
		 * The parameters can be changed in run-time, so they are saved in "init params"
		 * for 'disconnecting'.
		 * the parameters being set in setSite; "select" phase.
         */
		pQosMngr->acParams[acID].acQosParams.ac        = acID;
		pQosMngr->acParams[acID].acQosParams.aifsn     = AIFS_DEF;
		pQosMngr->acParams[acID].acQosParams.cwMax     = pQosMngr->uDesireCwMax;
		pQosMngr->acParams[acID].acQosParams.cwMin     = pQosMngr->uDesireCwMin;
		pQosMngr->acParams[acID].acQosParams.txopLimit = QOS_TX_OP_LIMIT_DEF;
		pQosMngr->acParams[acID].msduLifeTimeParam     = pQosMngrInitParams->MsduLifeTime[acID];

		/* The protocol is QOS_NONE. If Packet Burst is Enable,            */
		/* then, the BE queue is configured to the TxOP Limit of Packet burst */
		/* (that is, 3 ms) and the txopContinuation is set to  qosPacketBurstEnable  */
		/* The protocol is QOS_NONE. If Packet Burst is Enable,            */
		/* then, the BE queue is configured to the TxOP Limit of Packet burst */
		/* (that is, 3 ms) and the txopContinuation is set to  qosPacketBurstEnable  */

		if (acID == QOS_AC_BE)	
		{
			if (pQosMngr->qosPacketBurstEnable==TI_TRUE)
			{
				pQosMngr->acParams[QOS_AC_BE].acQosParams.txopLimit = pQosMngr->qosPacketBurstTxOpLimit;
			}
			else 
			{
				pQosMngr->acParams[QOS_AC_BE].acQosParams.txopLimit = QOS_TX_OP_LIMIT_DEF;
			}
		}

		/* 
		 * Update the acQosInitParams as well 
		 */
		os_memoryCopy(pQosMngr->hOs,
                      &pQosMngr->acParams[acID].acQosInitParams,
                      &pQosMngr->acParams[acID].acQosParams,
                      sizeof(TAcQosParams));

		/* will be config only after select */
		if(verifyAndConfigQosParams(hQosMngr,&(pQosMngr->acParams[acID].acQosParams)) != TI_OK)
		{
			TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_SetDefault: failed on verifyAndConfigQosParams\n");
		}

        /*
		 * setting ps mode per ac for protocol specific configuration.
		 */

        /* validity check - allow to set the desired Ps mode per-AC to UPSD ONLY IF the station supports UPSD */
        if ((pQosMngrInitParams->desiredPsMode == PS_SCHEME_UPSD_TRIGGER) && (pQosMngrInitParams->desiredWmeAcPsMode[acID] == PS_SCHEME_UPSD_TRIGGER))
        {
		        pQosMngr->acParams[acID].desiredWmeAcPsMode = PS_SCHEME_UPSD_TRIGGER;
        }
        else
        {
               pQosMngr->acParams[acID].desiredWmeAcPsMode = PS_SCHEME_LEGACY;
        }

		pQosMngr->acParams[acID].currentWmeAcPsMode  = PS_SCHEME_LEGACY; /* default configuration is legacy PS  for all queues */

		/* configure AC params to TxCtrl. */
		txCtrlParams_setAcMsduLifeTime(pQosMngr->hTxCtrl, acID, pQosMngrInitParams->MsduLifeTime[acID]);
		txCtrlParams_setAcAckPolicy(pQosMngr->hTxCtrl, acID, ACK_POLICY_LEGACY);

		/* setting wme Ack Policy */
		pQosMngr->acParams[acID].wmeAcAckPolicy = pQosMngrInitParams->acAckPolicy[acID];

		/* Set admission state per AC for non-QoS and update the Tx module. */
		setNonQosAdmissionState(pQosMngr, acID);
	}

    /* Reset all PS-Rx-Streaming configurations */
    for (uTid = 0; uTid < MAX_NUM_OF_802_1d_TAGS; uTid++) 
    {
        pQosMngr->aTidPsRxStreaming[uTid].uTid     = uTid;
        pQosMngr->aTidPsRxStreaming[uTid].bEnabled = TI_FALSE;
    }
    pQosMngr->uNumEnabledPsRxStreams = 0;

	/* update Tx header convert mode */
	txCtrlParams_setQosHeaderConverMode(pQosMngr->hTxCtrl, HDR_CONVERT_LEGACY);


    /* 802.11n BA session setting */
    for (uTid = 0; uTid < MAX_NUM_OF_802_1d_TAGS; ++uTid)
    {
        pQosMngr->aBaPolicy[uTid] = pQosMngrInitParams->aBaPolicy[uTid];
        pQosMngr->aBaInactivityTimeout[uTid] = pQosMngrInitParams->aBaInactivityTimeout[uTid];
    }

    

    TRACE0(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_config : QoS configuration complete!");

    return TI_OK;
}

/************************************************************************
 *                    qosMngr_resetAdmCtrlParameters	                *
 ************************************************************************
DESCRIPTION: reset the admCtrl parameters
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.

OUTPUT:		

RETURN:     

************************************************************************/

void qosMngr_resetAdmCtrlParameters(TI_HANDLE hQosMngr)
{
	TI_UINT8 acID;
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	/* reset admission control parameters */
	for(acID = FIRST_AC_INDEX ; acID < MAX_NUM_OF_AC ; acID++)
	{
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].AC = (EAcTrfcType)acID;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority = INACTIVE_USER_PRIORITY; /* Setting invalid user Priority to prevent GET_TSPEC or DELETE */
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].nominalMsduSize = 0;
        pQosMngr->resourceMgmtTable.currentTspecInfo[acID].minimumPHYRate = 0;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].meanDataRate = 0;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].surplausBwAllowance = 0;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].mediumTime = 0;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].UPSDFlag = 0;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].uMinimumServiceInterval = 0;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].uMaximumServiceInterval = 0;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].streamDirection = BI_DIRECTIONAL;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;

		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].AC = (EAcTrfcType)acID;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].userPriority = INACTIVE_USER_PRIORITY; /* Setting invalid user Priority to prevent GET_TSPEC or DELETE */
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].nominalMsduSize = 0;
        pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].minimumPHYRate = 0;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].meanDataRate = 0;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].surplausBwAllowance = 0;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].mediumTime = 0;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].UPSDFlag = 0;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].uMinimumServiceInterval = 0;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].uMaximumServiceInterval = 0;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].streamDirection = BI_DIRECTIONAL;
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;

		pQosMngr->resourceMgmtTable.totalAllocatedMediumTime = 0;
	}
}


/************************************************************************
 *                        qosMngr_disconnect   			                *
 ************************************************************************
DESCRIPTION: the function is called upon driver disconnecting to reset all
             QOS parameters to init values.
                                                                                                   
INPUT:      hQosMngr	-	Qos Manager handle.
            bDisconnect - True if full AP disconnection, False if roaming to another AP

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_disconnect (TI_HANDLE hQosMngr, TI_BOOL bDisconnect)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	TI_UINT32  acID;
	TI_STATUS  status;

	if(hQosMngr == NULL)
		return TI_OK;

	pQosMngr->activeProtocol    = QOS_NONE;
    pQosMngr->WMESiteSupport    = TI_FALSE;

	/* clear admission control params */
	qosMngr_resetAdmCtrlParameters(pQosMngr);
	
	trafficAdmCtrl_stop(pQosMngr->pTrafficAdmCtrl);

	for(acID = FIRST_AC_INDEX;acID < MAX_NUM_OF_AC; acID++)
	{
        /* Disable medium time events in TX */
		txCtrlParams_setAdmissionCtrlParams(pQosMngr->hTxCtrl, acID, 0 , 0, TI_FALSE);

		/* The protocol after disconnect is QOS_NONE. If Packet Burst is Enabled, the BE queue InitParams 
		    is configured to the TxOP Limit of Packet burst  (that is, 3 ms) and the 
		    txopContinuation is set to qosPacketBurstEnable. */
		
		if (acID == QOS_AC_BE)
		{
			if (pQosMngr->qosPacketBurstEnable==TI_TRUE)
			{
				pQosMngr->acParams[QOS_AC_BE].acQosInitParams.txopLimit = pQosMngr->qosPacketBurstTxOpLimit;
			}
			else 
			{
				pQosMngr->acParams[QOS_AC_BE].acQosInitParams.txopLimit = QOS_TX_OP_LIMIT_DEF;
			}
		}
		
		/* Copy init traffic params (non-QoS defaults) to current traffic params, and config to HAL and TNET. */
       os_memoryCopy(pQosMngr->hOs,&(pQosMngr->acParams[acID].acQosParams),&(pQosMngr->acParams[acID].acQosInitParams),sizeof(TAcQosParams));		

		/* 
		 * Update the qTrafficInitParams as well 
		 */
	   os_memoryCopy(pQosMngr->hOs,&(pQosMngr->acParams[acID].QtrafficParams),&(pQosMngr->acParams[acID].QTrafficInitParams),sizeof(TQueueTrafficParams));


	   pQosMngr->acParams[acID].currentWmeAcPsMode  = PS_SCHEME_LEGACY; /* default configuration is legacy PS  for all queues */

	   /* configure Ack-Policy to TxCtrl (working in Non-QoS method). */
	   txCtrlParams_setAcAckPolicy(pQosMngr->hTxCtrl, acID, ACK_POLICY_LEGACY);

	   /* Set admission state per AC for non-QoS and update the Tx module. */
	   setNonQosAdmissionState(pQosMngr, acID);
	}

	/* 
	 * configure only BE AC 
	 * NOTE : this is done after "disconnect" or Init phase so those are defaults BE params 
	 */

	/*
	 * configureQueue
	 */
	status = verifyAndConfigTrafficParams(hQosMngr,&(pQosMngr->acParams[QOS_AC_BE].QtrafficParams));
	if (status != TI_OK)
	{
        TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setSite:failed to init NON_QOS Queue Traffic parameters!!!\n\n");
		return status;
	}

	/*
	 * configureAC
	 */

	status = verifyAndConfigQosParams(hQosMngr,&(pQosMngr->acParams[QOS_AC_BE].acQosParams));
	if (status != TI_OK)
	{
        TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setSite:failed to init NON_QOS AC QoS parameters!!!\n\n");
        return status;
	}

	/* update Tx header convert mode */
	txCtrlParams_setQosHeaderConverMode(pQosMngr->hTxCtrl, HDR_CONVERT_LEGACY);

    /* If disconnect (not roaming), reset all PS-Rx-Streaming configurations. */
    if (bDisconnect) 
    {
        TI_UINT32  uTid;
        for (uTid = 0; uTid < MAX_NUM_OF_802_1d_TAGS; uTid++) 
        {
            TPsRxStreaming *pCurrTidParams = &pQosMngr->aTidPsRxStreaming[uTid];

            if (pCurrTidParams->bEnabled) 
            {
                pCurrTidParams->bEnabled = TI_FALSE;
                TWD_CfgPsRxStreaming (pQosMngr->hTWD, pCurrTidParams, NULL, NULL);
            }
        }
        pQosMngr->uNumEnabledPsRxStreams = 0;
    }

    /* Update our internal state */
    pQosMngr->isConnected = TI_FALSE;

    /* Mark that no Qos Null template is currently set into firmware */
    pQosMngr->QosNullDataTemplateUserPriority = 0xFF;

    pQosMngr->voiceTspecConfigured = TI_FALSE;
    pQosMngr->videoTspecConfigured = TI_FALSE;

    /* Mark that no Qos Null template is currently set into firmware */
    pQosMngr->QosNullDataTemplateUserPriority = 0xFF;

TRACE0(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_disconnect : QoS disconnect complete!");

#ifdef XCC_MODULE_INCLUDED
	measurementMgr_stopTsMetrics(pQosMngr->hMeasurementMngr);
#endif

	return TI_OK;
}


/************************************************************************
 *                        qosMngr_connect   			                *
 ************************************************************************
DESCRIPTION: the function is called upon driver connection to inform all 
             the other modules about the voice mode
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS qosMngr_connect(TI_HANDLE hQosMngr)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
    psPollTemplate_t        psPollTemplate;
    TSetTemplate            templateStruct;
    QosNullDataTemplate_t   QosNullDataTemplate;
    TI_UINT8   acID,UPSDCnt=0;

   if (pQosMngr->isConnected == TI_TRUE)
   {
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_connect : Already connected !!!\n");
     return TI_OK;
   }

    /* Send PsPoll template to HAL */
    
    templateStruct.ptr = (TI_UINT8 *)&psPollTemplate;
    templateStruct.type = PS_POLL_TEMPLATE;
    templateStruct.uRateMask = RATE_MASK_UNSPECIFIED;
    buildPsPollTemplate(pQosMngr->hSiteMgr, &templateStruct);
    TWD_CmdTemplate (pQosMngr->hTWD, &templateStruct, NULL, NULL);
    	
    /* Update our internal state */
    pQosMngr->isConnected = TI_TRUE;

    /* Set Qos-Null Data template into firmware */
	for(acID = FIRST_AC_INDEX;acID < MAX_NUM_OF_AC; acID++)
	{
        /* Set QOS Null data template into the firmware (only if at least one AC is configured as UPSD )*/
        if (pQosMngr->acParams[acID].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
        {
           UPSDCnt++;
           if ( pQosMngr->acParams[acID].apInitAdmissionState != ADMISSION_REQUIRED )
           {
            pQosMngr->QosNullDataTemplateUserPriority = WMEQosAcToTid[acID];

            templateStruct.ptr = (TI_UINT8 *)&QosNullDataTemplate;
            templateStruct.type = QOS_NULL_DATA_TEMPLATE;
            templateStruct.uRateMask = RATE_MASK_UNSPECIFIED;
            buildQosNullDataTemplate(pQosMngr->hSiteMgr, &templateStruct,pQosMngr->QosNullDataTemplateUserPriority);
            TWD_CmdTemplate (pQosMngr->hTWD, &templateStruct, NULL, NULL);

            TRACE2(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "setWmeSiteParams: Setting QOS Null data for UserPriority %d (belongs to AC %d)\n", WMEQosAcToTid[acID], acID);

            break; /* Only need to set ONE template, so after setting it, we can exit the loop */
           }
        }

    }

    /* If MAX_NUM_OF_AC (4) ACs were found as UPSD, but we still haven't configured any UP in the Qos Null data template, it must mean all ACs require admission - not valid*/
    if ((pQosMngr->QosNullDataTemplateUserPriority == 0xFF) && (UPSDCnt == MAX_NUM_OF_AC))
    {
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_connect : QOS Null Data template not set since all ACs require admission !!!\n");
	}

    return TI_OK;
}

/** 
 * \fn     qosMngr_SetBaPolicies
 * \brief  Set the BA session policies to the FW. 
 * 
 * \note   
 * \param  hQosMngr	- Qos Manager handle.
 * \return None 
 * \sa     
 */ 
void qosMngr_SetBaPolicies(TI_HANDLE hQosMngr)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
    TI_BOOL     b11nEnable;
    TI_UINT32   uTidIndex;
    paramInfo_t param;

    StaCap_IsHtEnable(pQosMngr->hStaCap, &b11nEnable);

    if (b11nEnable)
    {

        param.paramType = CTRL_DATA_CURRENT_BSSID_PARAM;
        ctrlData_getParam(pQosMngr->hCtrlData, &param);
 
        /* 802.11n BA session setting */
        for (uTidIndex = 0; uTidIndex < MAX_NUM_OF_802_1d_TAGS; ++uTidIndex)
        {
            if ((pQosMngr->aBaPolicy[uTidIndex] == BA_POLICY_INITIATOR) || 
                (pQosMngr->aBaPolicy[uTidIndex] == BA_POLICY_INITIATOR_AND_RECEIVER))
            {
                TWD_CfgSetBaInitiator (pQosMngr->hTWD,
                                       uTidIndex,
                                       TI_TRUE,
                                       param.content.ctrlDataCurrentBSSID,
                                       RX_QUEUE_WIN_SIZE,
                                       pQosMngr->aBaInactivityTimeout[uTidIndex]);
            }

            if ((pQosMngr->aBaPolicy[uTidIndex] == BA_POLICY_RECEIVER) ||
                (pQosMngr->aBaPolicy[uTidIndex] == BA_POLICY_INITIATOR_AND_RECEIVER))
            {
                TWD_CfgSetBaReceiver (pQosMngr->hTWD,
                                      uTidIndex,
                                      TI_TRUE,
                                      param.content.ctrlDataCurrentBSSID,
                                      RX_QUEUE_WIN_SIZE);
            }
        }
    }
}


/************************************************************************
 *                        qosMngr_evalSite					            *
 ************************************************************************
DESCRIPTION: Evaluate the site for the selction algorithm
			 In case the station is configure to work in UPSD mode
			 prefer a site that support UPSD and return 1.
			 All other case return 0.
                                                                                                   
INPUT:      siteAPSDSupport - the UPSD capabilit of the site

OUTPUT:		 

RETURN:     1 - evaluation is good...
			0 - evaluation can be better....

************************************************************************/
TI_UINT8 qosMngr_evalSite(TI_HANDLE hQosMngr, TI_BOOL siteAPSDSupport)
{
   qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

   if (pQosMngr->desiredPsMode == PS_SCHEME_UPSD_TRIGGER && siteAPSDSupport)
   {
		return 1;
   }

   return 0;
}

TI_STATUS qosMngr_getParamsActiveProtocol(TI_HANDLE hQosMngr, EQosProtocol *actProt)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	if (pQosMngr == NULL)
		return TI_NOK;
    *actProt = pQosMngr->activeProtocol;
    return TI_OK;
}

/************************************************************************
 *                        qosMngr_getACparams           			    *
 ************************************************************************
DESCRIPTION: The function is an API for external modules to qet qos parameters
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.
            pParamInfo           -  qos parameters information.


OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS qosMngr_getParams(TI_HANDLE  hQosMngr,paramInfo_t *pParamInfo)
{
	EAcTrfcType           acID;
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	if(pQosMngr == NULL)
		return TI_NOK;

    TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_getParams: %x\n", pParamInfo->paramType);

	switch(pParamInfo->paramType)
	{
	case QOS_PACKET_BURST_ENABLE:
		pParamInfo->content.qosPacketBurstEnb = pQosMngr->qosPacketBurstEnable;
		break;
	case QOS_MNGR_CURRENT_PS_MODE:
		pParamInfo->content.currentPsMode = pQosMngr->currentPsMode;
		break;

    case QOS_MNGR_ACTIVE_PROTOCOL:
       pParamInfo->content.qosSiteProtocol = pQosMngr->activeProtocol;
       break;

    case QOS_MNGR_GET_DESIRED_PS_MODE:
        pParamInfo->content.qosDesiredPsMode.uDesiredPsMode = pQosMngr->desiredPsMode;
		for(acID = FIRST_AC_INDEX; acID < MAX_NUM_OF_AC ; acID++ )
			pParamInfo->content.qosDesiredPsMode.uDesiredWmeAcPsMode[acID] = pQosMngr->acParams[acID].desiredWmeAcPsMode;
        break;
	   
	case QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC:
	/* Check if voice call present. If so, store current TSPEC configuration */
		pParamInfo->content.TspecConfigure.voiceTspecConfigure = (TI_UINT8)pQosMngr->voiceTspecConfigured;
        pParamInfo->content.TspecConfigure.videoTspecConfigure = (TI_UINT8)pQosMngr->videoTspecConfigured;

        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_getParams: QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC=%d\n", pQosMngr->voiceTspecConfigured);

		if (pQosMngr->voiceTspecConfigured == TI_TRUE) 
		{
			OS_802_11_QOS_TSPEC_PARAMS *pTspecParams;
			tspecInfo_t *pConfiguredParams;

			/* Store voice TSPEC params - must be configured */
			pTspecParams = &pQosMngr->tspecRenegotiationParams[USER_PRIORITY_6];
			pConfiguredParams = &pQosMngr->resourceMgmtTable.candidateTspecInfo[USER_PRIORITY_6];

			pTspecParams->uUserPriority = pConfiguredParams->userPriority;
			pTspecParams->uNominalMSDUsize = pConfiguredParams->nominalMsduSize;
			pTspecParams->uMeanDataRate = pConfiguredParams->meanDataRate;
			pTspecParams->uMinimumPHYRate = pConfiguredParams->minimumPHYRate;
			pTspecParams->uSurplusBandwidthAllowance = pConfiguredParams->surplausBwAllowance;
			pTspecParams->uAPSDFlag = pConfiguredParams->UPSDFlag;
			pTspecParams->uMinimumServiceInterval = pConfiguredParams->uMinimumServiceInterval;
			pTspecParams->uMaximumServiceInterval = pConfiguredParams->uMaximumServiceInterval;
			pTspecParams->uMediumTime = pConfiguredParams->mediumTime;
		}
		else
		{
			pQosMngr->tspecRenegotiationParams[USER_PRIORITY_6].uUserPriority = MAX_USER_PRIORITY;
		}

		if (pQosMngr->videoTspecConfigured == TI_TRUE) 
		{
			OS_802_11_QOS_TSPEC_PARAMS *pTspecParams;
			tspecInfo_t *pConfiguredParams;

			/* Store signalling TSPEC params if configured in user priority 4 */
			pTspecParams = &pQosMngr->tspecRenegotiationParams[USER_PRIORITY_4];
			pConfiguredParams = &pQosMngr->resourceMgmtTable.candidateTspecInfo[USER_PRIORITY_4];

				pTspecParams->uUserPriority = pConfiguredParams->userPriority;
				pTspecParams->uNominalMSDUsize = pConfiguredParams->nominalMsduSize;
				pTspecParams->uMeanDataRate = pConfiguredParams->meanDataRate;
				pTspecParams->uMinimumPHYRate = pConfiguredParams->minimumPHYRate;
				pTspecParams->uSurplusBandwidthAllowance = pConfiguredParams->surplausBwAllowance;
				pTspecParams->uAPSDFlag = pConfiguredParams->UPSDFlag;
				pTspecParams->uMinimumServiceInterval = pConfiguredParams->uMinimumServiceInterval;
				pTspecParams->uMaximumServiceInterval = pConfiguredParams->uMaximumServiceInterval;
				pTspecParams->uMediumTime = pConfiguredParams->mediumTime;
			}
			else
			{
				pQosMngr->tspecRenegotiationParams[USER_PRIORITY_4].uUserPriority = MAX_USER_PRIORITY;
		}
		break;
		
	case QOS_MNGR_AC_STATUS:
		switch (qosMngr_getCurrAcStatus (hQosMngr,&pParamInfo->content.qosCurrentAcStatus))
		{
   			case TI_OK:
      			return TI_OK;
			case NOT_CONNECTED:
                TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Not connected to an AP...\n");
				break;
   			case NO_QOS_AP:
                TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "AP does not support QOS...\n");
      			break;
   			case TI_NOK:
                TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Invalid parameter...\n");
      			break;
   			default:
                TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Unknown return value...\n");
      			break;			
   		}
		return TI_NOK;
		
	case QOS_MNGR_OS_TSPEC_PARAMS: 

		if( pParamInfo->content.qosTspecParameters.uUserPriority > MAX_USER_PRIORITY )
		{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getTspecParams: userPriority > 7 -> Ignore !!!\n");
			return TI_NOK;
		}

		if(pQosMngr->isConnected == TI_FALSE)
		{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getTspecParams: Not connected - Ignoring request !!!\n");
			return NOT_CONNECTED;
		}

		if(pQosMngr->activeProtocol == QOS_NONE)
		{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getTspecParams: Not connected to QOS AP - Ignoring reqeust !!!\n");
			return NO_QOS_AP;
		}
		
		acID = (EAcTrfcType)WMEQosTagToACTable[pParamInfo->content.qosTspecParameters.uUserPriority];

		/* check if signaling is already in process*/
		if(pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState == AC_WAIT_ADMISSION)
		{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_requestAdmission: AC = %d , TSPEC Signaling is in progress -> Ignoring request !!!\n",acID);
			return TRAFIC_ADM_PENDING;
		}

	   /* check if UP is admitted or not */
	   if(pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority != pParamInfo->content.qosTspecParameters.uUserPriority)
       {	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getTspecParams: user priority is not admitted. -> Ignore !!!\n");
		 return USER_PRIORITY_NOT_ADMITTED;
		}

		pParamInfo->content.qosTspecParameters.uMeanDataRate = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].meanDataRate;
		pParamInfo->content.qosTspecParameters.uNominalMSDUsize = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].nominalMsduSize;
		pParamInfo->content.qosTspecParameters.uAPSDFlag  = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].UPSDFlag;
		pParamInfo->content.qosTspecParameters.uMinimumServiceInterval  = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].uMinimumServiceInterval;
		pParamInfo->content.qosTspecParameters.uMaximumServiceInterval  = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].uMaximumServiceInterval;
		pParamInfo->content.qosTspecParameters.uMinimumPHYRate  = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].minimumPHYRate;
		pParamInfo->content.qosTspecParameters.uSurplusBandwidthAllowance  = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].surplausBwAllowance;
		pParamInfo->content.qosTspecParameters.uMediumTime = pQosMngr->resourceMgmtTable.currentTspecInfo[acID].mediumTime;
		break;

	case QOS_MNGR_AP_QOS_PARAMETERS:  /* API GetAPQosParameters */
		acID = (EAcTrfcType) pParamInfo->content.qosApQosParams.uAC;

		if(acID > QOS_HIGHEST_AC_INDEX)
		{
            TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setParams :Error  trying to set invalid acId: %d param\n",pParamInfo->content.qosApQosParams.uAC);
			return (PARAM_VALUE_NOT_VALID);
		}
		if(pQosMngr->isConnected == TI_FALSE)
		{
            TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Not connected to an AP...\n");
			return NOT_CONNECTED;
		}
		if(pQosMngr->activeProtocol == QOS_NONE)
		{
            TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "AP does not support QOS...\n");
			return NO_QOS_AP;
		}
		
		pParamInfo->content.qosApQosParams.uAssocAdmissionCtrlFlag = pQosMngr->acParams[acID].apInitAdmissionState; /* admission flag */
		pParamInfo->content.qosApQosParams.uAIFS = pQosMngr->acParams[acID].acQosParams.aifsn;
		pParamInfo->content.qosApQosParams.uCwMin = (1 << pQosMngr->acParams[acID].acQosParams.cwMin)-1;
		pParamInfo->content.qosApQosParams.uCwMax = (1 << pQosMngr->acParams[acID].acQosParams.cwMax)-1;
		pParamInfo->content.qosApQosParams.uTXOPLimit = pQosMngr->acParams[acID].acQosParams.txopLimit << 5;

		break;

    case QOS_MNGR_PS_RX_STREAMING:
        {
            TPsRxStreaming *pParams    = &pParamInfo->content.tPsRxStreaming;
            TI_UINT32       uTid       = pParams->uTid;
            TPsRxStreaming *pTidStream = &pQosMngr->aTidPsRxStreaming[uTid];

            os_memoryCopy (pQosMngr->hOs, (void *)pParams, (void *)pTidStream, sizeof(TPsRxStreaming));
        }
		break;


	default:
           TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getParams Error: unknown paramType 0x%x!\n",pParamInfo->paramType);
			return (PARAM_NOT_SUPPORTED);

	}
	return TI_OK;
}

/************************************************************************
 *                        qosMngr_setParams              			    *
 ************************************************************************
DESCRIPTION: The function is an API for external modules to set qos parameters
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.
            pParamInfo           -  qos parameters information.


OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS qosMngr_setParams(TI_HANDLE  hQosMngr,paramInfo_t *pParamInfo)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	TTwdParamInfo		   param;		
	EAcTrfcType           acID;
	TI_STATUS              status;

	if(pQosMngr == NULL)
		return TI_NOK;

	if(pParamInfo == NULL)
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setParams :Error trying to set NULL params!\n");
		return TI_NOK;
	}

TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_setParams: %x\n", pParamInfo->paramType);

	switch(pParamInfo->paramType)
	{

		case QOS_PACKET_BURST_ENABLE:

			if (pParamInfo->content.qosPacketBurstEnb > QOS_PACKET_BURST_ENABLE_MAX)
				return (PARAM_VALUE_NOT_VALID);
			
			/* No change */
			if (pParamInfo->content.qosPacketBurstEnb == pQosMngr->qosPacketBurstEnable)
				return TI_OK;

			/* Update the qosPacketBurstEnable parameter */
			pQosMngr->qosPacketBurstEnable = pParamInfo->content.qosPacketBurstEnb;

			/* Packet burst enable changed from F to T */ 
			if (pParamInfo->content.qosPacketBurstEnb == TI_TRUE)
			{
				/* Update the acTrafficInitParams of BE to the packet burst def*/
				pQosMngr->acParams[QOS_AC_BE].acQosInitParams.txopLimit = pQosMngr->qosPacketBurstTxOpLimit;
			
				/* Update the acTrafficParams of BE and the hal to the packet burst def*/
				if (pQosMngr->activeProtocol == QOS_NONE)
				{
					pQosMngr->acParams[QOS_AC_BE].acQosParams.txopLimit = pQosMngr->qosPacketBurstTxOpLimit;
					/* verify the parameters and update the hal */
					status = verifyAndConfigQosParams(hQosMngr,&(pQosMngr->acParams[QOS_AC_BE].acQosParams));
					if(status != TI_OK)
						return status;
				}
			}
				
			/* Packet burst enable changed from T to F*/ 
			else 
			{
				/* Update the acTrafficInitParams of BE to the AC def*/
				pQosMngr->acParams[QOS_AC_BE].acQosInitParams.txopLimit = QOS_TX_OP_LIMIT_DEF;
			
				/* Update the acTrafficParams of BE  and the hal to the AC def*/
				if (pQosMngr->activeProtocol == QOS_NONE)
				{
					pQosMngr->acParams[QOS_AC_BE].acQosParams.txopLimit = QOS_TX_OP_LIMIT_DEF;
					/* verify the parameters and update the hal */
					status = verifyAndConfigQosParams(hQosMngr,&(pQosMngr->acParams[QOS_AC_BE].acQosParams));
					if(status != TI_OK)
						return status;
				}
			}
			break;

		case QOS_MNGR_SET_SITE_PROTOCOL:
			if(pParamInfo->content.qosSiteProtocol == QOS_WME)
				pQosMngr->WMESiteSupport = TI_TRUE;
			else
				pQosMngr->WMESiteSupport = TI_FALSE;
		break;


    case QOS_MNGR_PS_RX_STREAMING:
        return qosMngr_SetPsRxStreaming (pQosMngr, &pParamInfo->content.tPsRxStreaming);


	case QOS_MNGR_SET_OS_PARAMS:
		if((EAcTrfcType)pParamInfo->content.qosOsParams.acID > QOS_HIGHEST_AC_INDEX)
		{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setParams :Error  trying to set invalid acId: %d param\n",pParamInfo->content.qosOsParams.acID);

			return (PARAM_VALUE_NOT_VALID);
		}

		if(((PSScheme_e)pParamInfo->content.qosOsParams.PSDeliveryProtocol != PS_SCHEME_LEGACY) && ((PSScheme_e)pParamInfo->content.qosOsParams.PSDeliveryProtocol != PS_SCHEME_UPSD_TRIGGER))
		{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setParams :Error trying to set invalid PSDeliveryProtocol: %d param\n",pParamInfo->content.qosOsParams.PSDeliveryProtocol);

			return (PARAM_VALUE_NOT_VALID);
		}

		/* config tidConf */
		acID = (EAcTrfcType)pParamInfo->content.qosOsParams.acID;

		if( (pParamInfo->content.qosOsParams.PSDeliveryProtocol != pQosMngr->acParams[acID].desiredWmeAcPsMode) && 
			(pQosMngr->isConnected == TI_TRUE) )
		{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setParams :Error  trying to set new PS protocol while connected");
			
			return (PARAM_VALUE_NOT_VALID);
		}


		/* UPSD_FW open in upsd integration */
		/* set the current PS mode. In not connected state it is always Legacy since the currentPsMode only 
		 update after connection */
		pQosMngr->acParams[acID].QtrafficParams.psScheme = pQosMngr->acParams[acID].currentWmeAcPsMode;
		pQosMngr->acParams[acID].msduLifeTimeParam = pParamInfo->content.qosOsParams.MaxLifeTime;

		status = verifyAndConfigTrafficParams(pQosMngr,&(pQosMngr->acParams[acID].QtrafficParams));
		if(status != TI_OK)
			return status;
		
		/* configure MSDU-Lifetime to TxCtrl. */
		txCtrlParams_setAcMsduLifeTime(pQosMngr->hTxCtrl, acID, pParamInfo->content.qosOsParams.MaxLifeTime);

		/* synch psPoll mode with qosMngr */
		/* Update the PsMode parameter */
		pQosMngr->acParams[acID].desiredWmeAcPsMode = (PSScheme_e) pParamInfo->content.qosOsParams.PSDeliveryProtocol;
		break;

	case QOS_MNGR_CURRENT_PS_MODE:
		if( (pQosMngr->activeProtocol == QOS_WME) && (pQosMngr->desiredPsMode == PS_SCHEME_UPSD_TRIGGER) && (pParamInfo->content.currentPsMode == PS_SCHEME_UPSD_TRIGGER) )
			pQosMngr->currentPsMode = PS_SCHEME_UPSD_TRIGGER;
		else
			pQosMngr->currentPsMode = PS_SCHEME_LEGACY;
		break;
		
    case QOS_MNGR_ADD_TSPEC_REQUEST:
		pQosMngr->TSPECNegotiationResultCallb = NULL;
		pQosMngr->TSPECNegotiationResultModule = NULL;
		status = qosMngr_requestAdmission(hQosMngr,  &pParamInfo->content.qosAddTspecRequest);
		switch (status)
   		{
   			case TI_OK:
      			return TI_OK;
      			
   			case TRAFIC_ADM_PENDING:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Driver is still waiting for a response of previous request...\n");
      			break;
   			case AC_ALREADY_IN_USE:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Other user priority from the same AC has already used a TSPEC...\n");
      			break;
   			case NOT_CONNECTED:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Not connected to an AP...\n");
      			break;
   			case NO_QOS_AP:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "AP does not support QOS...\n");
      			break;
   			case TI_NOK:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Invalid parameter...\n");
      			break;
   			default:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Unknown return value...\n");
      			break;
   		}
		return TI_NOK;
		
	case QOS_MNGR_RESEND_TSPEC_REQUEST:
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_setParams: QOS_MNGR_RESEND_TSPEC_REQUEST\n");
		pQosMngr->TSPECNegotiationResultCallb = (qosMngrCallb_t)pParamInfo->content.qosRenegotiateTspecRequest.callback;
		pQosMngr->TSPECNegotiationResultModule = pParamInfo->content.qosRenegotiateTspecRequest.handler; 
		status = qosMngr_requestAdmission(hQosMngr,  &pQosMngr->tspecRenegotiationParams[USER_PRIORITY_6]);

		if ((status == TI_OK) && (pQosMngr->tspecRenegotiationParams[USER_PRIORITY_4].uUserPriority != MAX_USER_PRIORITY))
		{
			status = qosMngr_requestAdmission(hQosMngr,  &pQosMngr->tspecRenegotiationParams[USER_PRIORITY_4]);
		}
		return (status);
		
    case QOS_MNGR_DEL_TSPEC_REQUEST:
		status = qosMngr_deleteAdmission(hQosMngr, &pParamInfo->content.qosDelTspecRequest);
		switch (status)
   		{
   			case TI_OK:
      			return TI_OK;
			case NOT_CONNECTED:
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Not connected to an AP...\n");
				break;
   			case NO_QOS_AP:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "AP does not support QOS...\n");
      			break;
   			case TI_NOK:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Invalid parameter...\n");
      			break;
   			default:
      TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Unknown return value...\n");
      			break;			
   		}
		return TI_NOK;

	case QOS_SET_RX_TIME_OUT:
		if (pParamInfo->content.rxTimeOut.UPSD == 0)
		{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, " :Error trying to set invalid zero timeout for UPSD \n");
				return PARAM_VALUE_NOT_VALID;
				
		}
		pQosMngr->rxTimeOut.psPoll = (TI_UINT16)pParamInfo->content.rxTimeOut.psPoll;
		pQosMngr->rxTimeOut.UPSD = (TI_UINT16)pParamInfo->content.rxTimeOut.UPSD;


		/* set RxTimeOut to FW */
		param.paramType	= TWD_RX_TIME_OUT_PARAM_ID;
		param.content.halCtrlRxTimeOut = pQosMngr->rxTimeOut;
		TWD_SetParam (pQosMngr->hTWD, &param); 
		break;

	case QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC:

		if( pParamInfo->content.TspecConfigure.voiceTspecConfigure || pParamInfo->content.TspecConfigure.videoTspecConfigure)
            pQosMngr->performTSPECRenegotiation = TI_TRUE;
		else
			pQosMngr->performTSPECRenegotiation = TI_FALSE;

TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_setParams: QOS_MNGR_VOICE_RE_NEGOTIATE_TSPEC=%d\n", pQosMngr->performTSPECRenegotiation);
	   break;

	default:
         TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getParams Error: unknown paramType 0x%x!\n",pParamInfo->paramType);
			return (PARAM_NOT_SUPPORTED);
	}

	return TI_OK;


}

/************************************************************************
 *                        verifyAndConfigTrafficParams  			    *
 ************************************************************************
DESCRIPTION: The function verifies the parameters set by qosMngr to 
             the queue traffic params in whalCtrl to be configured to TNET. 
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.
            pAcTrafficParams     -  pointer to ac parameters.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static TI_STATUS verifyAndConfigTrafficParams(qosMngr_t *pQosMngr, TQueueTrafficParams *pQtrafficParams)
{
    TTwdParamInfo		   param;    
    TQueueTrafficParams   queueTrafficParams;

	if(pQtrafficParams->queueID > MAX_NUM_OF_AC - 1)
    {
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigTrafficParams :Error  trying to set invalid queueID: %d param",pQtrafficParams->queueID);

		return (PARAM_VALUE_NOT_VALID);
	}


	if(pQtrafficParams->channelType > MAX_CHANNEL_TYPE)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigTrafficParams :Error  trying to set invalid channelType: %d param",pQtrafficParams->channelType);

		return (PARAM_VALUE_NOT_VALID);

	}

    /* TBD */
	if(pQtrafficParams->tsid > AC_PARAMS_MAX_TSID)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigTrafficParams :Error  trying to set invalid tsid: %d param",pQtrafficParams->tsid);

		return (PARAM_VALUE_NOT_VALID);

	}

	if(pQtrafficParams->psScheme > MAX_PS_SCHEME)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigTrafficParams :Error  trying to set invalid psScheme: %d param",pQtrafficParams->psScheme);

		return (PARAM_VALUE_NOT_VALID);
	}

	if(pQtrafficParams->ackPolicy > MAX_ACK_POLICY)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigTrafficParams :Error  trying to set invalid ackPolicy: %d param",pQtrafficParams->ackPolicy);

		return (PARAM_VALUE_NOT_VALID);
	}

    queueTrafficParams = *pQtrafficParams;

	param.paramType = TWD_QUEUES_PARAM_ID;
	/* set parameters */
	param.content.pQueueTrafficParams = &queueTrafficParams;

	return TWD_SetParam (pQosMngr->hTWD, &param);
}

/************************************************************************
 *                        verifyAndConfigQosParams          		    *
 ************************************************************************
DESCRIPTION: The function verifies the parameters set by qosMngr to 
             the AC Qos params in whalCtrl to be configured to TNET. 
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.
            pAcTrafficParams     -  pointer to ac parameters.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static TI_STATUS  verifyAndConfigQosParams(qosMngr_t *pQosMngr,TAcQosParams *pAcQosParams)
{
	TAcQosParams          acQosParams;

	if(pAcQosParams->ac >  MAX_NUM_OF_AC - 1 )
    {
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigQosParams :Error  trying to set invalid ac : %d param",pAcQosParams->ac);
        return (PARAM_VALUE_NOT_VALID);
	}
    /*  verification is unnecessary due to limited range of pAcQosParams->aifsn data type (TI_UINT8)
	if(pAcQosParams->aifsn >  QOS_AIFS_MAX )
    {
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigQosParams :Error  trying to set invalid aifsn : %d param",pAcQosParams->aifsn);

       return (PARAM_VALUE_NOT_VALID);
	}
    */
	if(pAcQosParams->cwMax >  QOS_CWMAX_MAX )
    {
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigQosParams :Error  trying to set invalid cwMax : %d param",pAcQosParams->cwMax);
        return (PARAM_VALUE_NOT_VALID);
	}

	if(pAcQosParams->cwMin >  QOS_CWMIN_MAX )
    {
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigQosParams :Error  trying to set invalid cwMax : %d param",pAcQosParams->cwMax);
        return (PARAM_VALUE_NOT_VALID);
	}

	if(pAcQosParams->txopLimit >  QOS_TX_OP_LIMIT_MAX )
    {
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "verifyAndConfigQosParams :Error  trying to set invalid txopLimit : %d param",pAcQosParams->txopLimit);
        return (PARAM_VALUE_NOT_VALID);
	}

	acQosParams.ac = pAcQosParams->ac;
	acQosParams.aifsn =  pAcQosParams->aifsn;

	/* convert to TNET units */
	acQosParams.cwMax =  (1 << pAcQosParams->cwMax) - 1; /* CwMax = 2^CwMax - 1*/
	acQosParams.cwMin =  (1 << pAcQosParams->cwMin) - 1; /* CwMin = 2^CwMin - 1*/
	acQosParams.txopLimit =  pAcQosParams->txopLimit << 5; /* in us */

	return TWD_CfgAcParams (pQosMngr->hTWD, &acQosParams, NULL, NULL);
}

/************************************************************************
 *                        qosMngr_GetWmeEnableFlag    			            *
 ************************************************************************
DESCRIPTION: The function is called in order to get the WME enable flag 
             of qosMngr according to init file desired mode.
             called from StaCap_GetHtCapabilitiesIe.
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.
            bWmeEnable           -  return flag.   

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_GetWmeEnableFlag(TI_HANDLE hQosMngr, TI_BOOL *bWmeEnable)
{
   	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	*bWmeEnable = pQosMngr->WMEEnable;

    return TI_OK;
}

/************************************************************************
 *                        qosMngr_selectActiveProtocol    			            *
 ************************************************************************
DESCRIPTION: The function is called in order to set the active protocol in
             the qosMngr according to site capabilities and desired mode.
             called from SystemConfig.
                                                                                                   
INPUT:      

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_selectActiveProtocol(TI_HANDLE  hQosMngr)
{
   	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	/* decide qos protocol */
	/* NOTE: if both XCC qnd wme supported wme is chosen */
	if(pQosMngr->WMESiteSupport && pQosMngr->WMEEnable)
	{
		pQosMngr->activeProtocol = QOS_WME;
	}
	else
    {
		pQosMngr->activeProtocol = QOS_NONE;
	}
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, " qosMngr_selectActiveProtocol() : pQosMngr->activeProtocol %d\n",pQosMngr->activeProtocol);

    return TI_OK;
}

/************************************************************************
 *                        qosMngr_setAcPsDeliveryMode    			            *
 ************************************************************************
DESCRIPTION: The function is called in order to set the upsd/ps_poll according 
             to the desired and current upsd mode (per AC as well).
             called from systemConfig.
                                                                                                   
INPUT:      

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_setAcPsDeliveryMode(TI_HANDLE  hQosMngr)
{
   TI_UINT8 acID;
   qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	/* in case the current PS mode is not UPSD  - the IE is empty */
	if(pQosMngr->currentPsMode == PS_SCHEME_UPSD_TRIGGER)
	{
		for(acID = FIRST_AC_INDEX;acID < MAX_NUM_OF_AC; acID++)
		{
			if(pQosMngr->acParams[acID].desiredWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
			{
				pQosMngr->acParams[acID].currentWmeAcPsMode = PS_SCHEME_UPSD_TRIGGER;
			}
		}
    }
	
	return TI_OK;

}


/************************************************************************
 *                        qosMngr_getQosCapabiltyInfeElement    			            *
 ************************************************************************
DESCRIPTION: The function is called in order to build the Qos Capability
			 IE for the associatiomn request.
                                                                                                   
INPUT:      

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_getQosCapabiltyInfeElement(TI_HANDLE  hQosMngr, TI_UINT8 *pQosIe, TI_UINT32 *pLen)
{    
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	dot11_QOS_CAPABILITY_IE_t *dot11_QOS_CAPABILITY_IE = (dot11_QOS_CAPABILITY_IE_t *)pQosIe;
	TI_STATUS status = TI_OK;
	TI_UINT8	extraIeLen = 0;
	
	if(pQosMngr->activeProtocol == QOS_WME)
	{
		dot11_QOS_CAPABILITY_IE->hdr[0]    = DOT11_QOS_CAPABILITY_ELE_ID;
		dot11_QOS_CAPABILITY_IE->hdr[1]   = DOT11_QOS_CAPABILITY_ELE_LEN;
		
		/* The default configuration of QoS info Field is legacy PS for all ACs */
		dot11_QOS_CAPABILITY_IE->QosInfoField = 0;
		
		/* in case the current PS mode is not UPSD  - the IE is empty */
		if(pQosMngr->currentPsMode == PS_SCHEME_UPSD_TRIGGER)
		{
			if(pQosMngr->acParams[QOS_AC_VO].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
			{
				dot11_QOS_CAPABILITY_IE->QosInfoField |= (1 << AC_VO_APSD_FLAGS_SHIFT);
			}
			if(pQosMngr->acParams[QOS_AC_VI].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
			{
				dot11_QOS_CAPABILITY_IE->QosInfoField |= (1 << AC_VI_APSD_FLAGS_SHIFT);
			}

			if(pQosMngr->acParams[QOS_AC_BK].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
			{
				dot11_QOS_CAPABILITY_IE->QosInfoField |= (1 << AC_BK_APSD_FLAGS_SHIFT);
			}

			if(pQosMngr->acParams[QOS_AC_BE].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
			{
				dot11_QOS_CAPABILITY_IE->QosInfoField |= (1 << AC_BE_APSD_FLAGS_SHIFT);
			}

			dot11_QOS_CAPABILITY_IE->QosInfoField |= (((pQosMngr->desiredMaxSpLen) & MAX_SP_LENGTH_MASK) << MAX_SP_LENGTH_SHIFT);

            TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "dot11_QOS_CAPABILITY_IE->QosInfoField = 0x%x\n",dot11_QOS_CAPABILITY_IE->QosInfoField);
		}

		*pLen = dot11_QOS_CAPABILITY_IE->hdr[1] + sizeof(dot11_eleHdr_t);
		
#ifdef XCC_MODULE_INCLUDED
		/* If required, add XCC info-elements to the association request packets */
		if (pQosMngr->performTSPECRenegotiation == TI_TRUE)
		{
            TRACE0(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_getQosCapabiltyInfeElement: performing TSPEC renegotiation\n");

			status = XCCMngr_getXCCQosIElements(pQosMngr->hXCCMgr, (pQosIe+(*pLen)), &extraIeLen);
		}
#endif
		*pLen += extraIeLen;
	}
	else
	{
		*pLen = 0;
	}

	return status;

}
/************************************************************************
 *                        qosMngr_assocReqBuild    			            *
 ************************************************************************
DESCRIPTION: The function is called in order to build the assocReq IE for
             the current site QOS protocol.
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS qosMngr_assocReqBuild(TI_HANDLE  hQosMngr, TI_UINT8 *pQosIe, TI_UINT32 *pLen)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	TI_STATUS		status;
	TI_UINT8 temp;

	
	if(pQosMngr == NULL)
	{
		*pLen = 0;
		return TI_OK;
	}	

	/* building assocReq frame */
	switch(pQosMngr->activeProtocol)
	{
	case QOS_WME:
		status = getWMEInfoElement(pQosMngr,pQosIe,&temp);
		if (status !=TI_OK) 
		{
			*pLen = 0;
		}
		*pLen = temp;
		break;

	case QOS_NONE:
			*pLen = 0;
			return TI_OK;

	default:
			*pLen = 0;
		break;
	}
	
	return TI_OK;
}

/************************************************************************
 *                        getWMEInfoElement     			            *
 ************************************************************************
DESCRIPTION: building QOS_WME IE.
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static TI_STATUS getWMEInfoElement(qosMngr_t *pQosMngr,TI_UINT8 *pWMEie,TI_UINT8 *pLen)
{
	dot11_WME_IE_t *pDot11_WME_IE = (dot11_WME_IE_t *)pWMEie;

	pDot11_WME_IE->hdr[0]         = DOT11_WME_ELE_ID;
	pDot11_WME_IE->hdr[1]        = DOT11_WME_ELE_LEN;
	pDot11_WME_IE->OUI[0]            = 0x00;
	pDot11_WME_IE->OUI[1]            = 0x50;
	pDot11_WME_IE->OUI[2]            = 0xf2;
	pDot11_WME_IE->OUIType           = dot11_WME_OUI_TYPE;
	pDot11_WME_IE->OUISubType        = dot11_WME_OUI_SUB_TYPE_IE;
	pDot11_WME_IE->version           = dot11_WME_VERSION;
	pDot11_WME_IE->ACInfoField       = 0;

	if(pQosMngr->currentPsMode == PS_SCHEME_UPSD_TRIGGER)
	{
		if(pQosMngr->acParams[QOS_AC_VO].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
		{
			pDot11_WME_IE->ACInfoField |= (1 << AC_VO_APSD_FLAGS_SHIFT);
		}
		if(pQosMngr->acParams[QOS_AC_VI].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
		{
			pDot11_WME_IE->ACInfoField |= (1 << AC_VI_APSD_FLAGS_SHIFT);
		}

		if(pQosMngr->acParams[QOS_AC_BK].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
		{
			pDot11_WME_IE->ACInfoField |= (1 << AC_BK_APSD_FLAGS_SHIFT);
		}

		if(pQosMngr->acParams[QOS_AC_BE].currentWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER)
		{
			pDot11_WME_IE->ACInfoField |= (1 << AC_BE_APSD_FLAGS_SHIFT);
		}

		pDot11_WME_IE->ACInfoField |= (((pQosMngr->desiredMaxSpLen) & MAX_SP_LENGTH_MASK) << MAX_SP_LENGTH_SHIFT);
	}

	*pLen = pDot11_WME_IE->hdr[1] + sizeof(dot11_eleHdr_t);

	return TI_OK;

}

/************************************************************************
 *                        qosMngr_checkTspecRenegResults		        *
 ************************************************************************
DESCRIPTION: The function is called upon association response to check 
            Tspec renegotiation results
                                                                                                   
INPUT:      hQosMngr	  -	Qos Manager handle.
            assocRsp      -  pointer to received IE parameters received 
			                 in association response. 
OUTPUT:		

RETURN:     -

************************************************************************/
void qosMngr_checkTspecRenegResults(TI_HANDLE hQosMngr, assocRsp_t *assocRsp)
{
	tspecInfo_t	tspecInfo;
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
#ifdef XCC_MODULE_INCLUDED
	TI_UINT32 acCount;
#endif

TRACE2(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_checkTspecRenegResults: performTSPECRenegotiation = %d, tspecParams received= %x\n",		pQosMngr->performTSPECRenegotiation, assocRsp->tspecVoiceParameters);

	if (pQosMngr->performTSPECRenegotiation != TI_TRUE)
	{
		/* If no re-negotiation was requested, no parsing shall be done */
#ifdef XCC_MODULE_INCLUDED
		measurementMgr_disableTsMetrics(pQosMngr->hMeasurementMngr, MAX_NUM_OF_AC);
#endif
		return;
	}

	if ( (assocRsp->tspecVoiceParameters == NULL) && (assocRsp->tspecSignalParameters == NULL) )
	{
		/* The renegotiation request was ignored - update QoS Manager database */
		qosMngr_setAdmissionInfo(pQosMngr, USER_PRIORITY_6, 
								 &pQosMngr->resourceMgmtTable.candidateTspecInfo[USER_PRIORITY_6], 
								 STATUS_TRAFFIC_ADM_REQUEST_REJECT);

		if (pQosMngr->tspecRenegotiationParams[USER_PRIORITY_4].uUserPriority != MAX_USER_PRIORITY)
		{
			qosMngr_setAdmissionInfo(pQosMngr, USER_PRIORITY_4, 
									 &pQosMngr->resourceMgmtTable.candidateTspecInfo[USER_PRIORITY_4], 
									 STATUS_TRAFFIC_ADM_REQUEST_REJECT);
		}
#ifdef XCC_MODULE_INCLUDED
        measurementMgr_disableTsMetrics(pQosMngr->hMeasurementMngr, MAX_NUM_OF_AC);
#endif
		return;
	}


	if (assocRsp->tspecVoiceParameters != NULL)
	{
	/* The renogitaion was performed - update QoS Manager database */
	pQosMngr->voiceTspecConfigured = TI_TRUE;
	trafficAdmCtrl_parseTspecIE(&tspecInfo, assocRsp->tspecVoiceParameters);

	qosMngr_setAdmissionInfo(pQosMngr, tspecInfo.AC, &tspecInfo, STATUS_TRAFFIC_ADM_REQUEST_ACCEPT);
	}

	if (assocRsp->tspecSignalParameters != NULL)
	{
		/* Signal TSPEC was re-negotiated as well */
		pQosMngr->videoTspecConfigured = TI_TRUE;
		trafficAdmCtrl_parseTspecIE(&tspecInfo, assocRsp->tspecSignalParameters);
		qosMngr_setAdmissionInfo(pQosMngr, tspecInfo.AC, &tspecInfo, STATUS_TRAFFIC_ADM_REQUEST_ACCEPT);
	}
	else if (pQosMngr->tspecRenegotiationParams[USER_PRIORITY_4].uUserPriority != MAX_USER_PRIORITY) 
	{
		/* Signal TSPEC was not re-negotiated although requested to - ERROR */
        TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setSite: Signal TSPEC was not re-negotiated while voice was \n");
		qosMngr_setAdmissionInfo(pQosMngr, USER_PRIORITY_4, 
								 &pQosMngr->resourceMgmtTable.candidateTspecInfo[USER_PRIORITY_4], 
								 STATUS_TRAFFIC_ADM_REQUEST_REJECT);
	}

#ifdef XCC_MODULE_INCLUDED
	/* If XCC IEs are present for one or more ACs, update other modules with received parameters */
	for (acCount = 0; acCount < MAX_NUM_OF_AC; acCount++)
	{
		XCCMngr_setXCCQoSParams(pQosMngr->hXCCMgr, &assocRsp->XCCIEs[acCount], acCount);
	}
#endif
}


/************************************************************************
 *                        qosMngr_setSite        			            *
 ************************************************************************
DESCRIPTION: The function is called upon association response to set site 
             parameters.
                                                                                                   
INPUT:      hQosMngr	  -	Qos Manager handle.
            assocRsp      -  pointer to received IE parameters received 
			                 in association response. 
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_setSite(TI_HANDLE hQosMngr, assocRsp_t *assocRsp)
{
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	TI_STATUS  status;

	if(hQosMngr == NULL)
        return TI_NOK;

	/* checking active protocol */
	switch(pQosMngr->activeProtocol)
	{
		case QOS_WME:
			/* verify QOS protocol received in association response */
			status = verifyWmeIeParams(pQosMngr, (TI_UINT8 *)assocRsp->WMEParams);
			if(status != TI_OK)
			{
                pQosMngr->activeProtocol = QOS_NONE;
                TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setSite: setting active protocol QOS_WME params with non QOS_WME IE params frame, setting active protocol back to NONE \n");
                status = qosMngr_setSite(hQosMngr, assocRsp);
                return status;
			}

            status = setWmeSiteParams(pQosMngr, (TI_UINT8 *)assocRsp->WMEParams);
			if (status != TI_OK)
			{
                pQosMngr->activeProtocol = QOS_NONE;
                TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "Warning: qosMngr_setSite-> failed to set AC QOS_WME parameters!!! , setting active protocol back to NONE\n");
                return TI_NOK;
			}
			/* update siteMgr with recevied params */
			status = siteMgr_setWMEParamsSite(pQosMngr->hSiteMgr, assocRsp->WMEParams);
			if (status != TI_OK)
			{
                pQosMngr->activeProtocol = QOS_NONE;
                TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setSite:failed to init QOS_WME parameters!!! , setting active protocol back to NONE\n\n");
                return TI_NOK;
			}

			break;

	case QOS_NONE:

			/* Check if the packet burst is enable, if it is, 
			should update the BE parames and the hal to the packet burst def */
			if (pQosMngr->qosPacketBurstEnable == TI_TRUE)
			{
				/* Update the acTrafficInitParams of BE to the packet burst def*/
				pQosMngr->acParams[QOS_AC_BE].acQosInitParams.txopLimit = pQosMngr->qosPacketBurstTxOpLimit;
				/* Update the acTrafficParams of BE to the packet burst def*/
				pQosMngr->acParams[QOS_AC_BE].acQosInitParams.txopLimit = pQosMngr->qosPacketBurstTxOpLimit;
				/* verify the parameters and update the hal */
				status = verifyAndConfigQosParams(hQosMngr,&(pQosMngr->acParams[QOS_AC_BE].acQosParams));
				if (status != TI_OK)
				{
                    TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "qosMngr_setSite:failed to init NON_QOS parameters!!!\n\n");
					return TI_NOK;
				}
			}

		break;

	default:
        TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "Warning: qosMngr_setSite NO active protocls To set \n");
		break;
	}

	/* Check if TSPEC re-negotiation was performed, if so - look for results */
	qosMngr_checkTspecRenegResults(pQosMngr, assocRsp);
 
    return TI_OK;

}

/************************************************************************
 *                        qosMngr_updateIEinfo     			            *
 ************************************************************************
DESCRIPTION: The function is called upon run-time update of AC parameters
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.
            pQosIeParams         -  pointer to received IE parameters received 
			                        in beacon or probe response. 
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

void qosMngr_updateIEinfo(TI_HANDLE hQosMngr, TI_UINT8 *pQosIeParams, EQosProtocol qosSetProtocol)
{
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	TI_STATUS  status;
	dot11_WME_PARAM_t		*pWMEparams;



	if(pQosMngr == NULL)
		return;

	/* checking active protocol */
	switch(pQosMngr->activeProtocol)
	{
	case QOS_WME:
		if(qosSetProtocol != QOS_WME)
			return;

		if(pQosIeParams == NULL)
		{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "Warning: updateIEinfo -> trying to update QOS_WME parameters with NULL site parameters!!!\n");
			return ;
		}

		/* verify QOS protocol received in update IE */
		status = verifyWmeIeParams(pQosMngr,pQosIeParams);
		if(status != TI_OK)
		{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "Warning: updateIEinfo ->upadting active protocl QOS_WME params with non QOS_WME IE params frame !!!\n");
			return ;
		}
		pWMEparams = (dot11_WME_PARAM_t *)pQosIeParams;

		/* update AC params */
		status = updateACParams(pQosMngr,&(pWMEparams->WME_ACParameteres));
		if(status != TI_OK)
		{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "updateIEinfo-> failed to update AC QOS_WME parameters!!!\n\n");
			return ;
		}
		break;


	default:
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "updateIEinfo-> trying to update qos paramters without active protocol !!!");
		break;
	}
}

/************************************************************************
 *                        qosMngr_buildTSPec       			            *
 ************************************************************************/
TI_UINT32 qosMngr_buildTSPec(TI_HANDLE hQosMngr, TI_UINT32 user_priority, TI_UINT8 *pQosIe)
{
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	OS_802_11_QOS_TSPEC_PARAMS *pPreservedParams;
	tspecInfo_t *pCandidateParams;
	TI_UINT32 ieLen;

	pPreservedParams = &pQosMngr->tspecRenegotiationParams[user_priority];
	pCandidateParams = &pQosMngr->resourceMgmtTable.candidateTspecInfo[user_priority];

	if (pPreservedParams->uUserPriority != MAX_USER_PRIORITY)
	{
		qosMngr_storeTspecCandidateParams (pCandidateParams, pPreservedParams, user_priority);
		pCandidateParams->trafficAdmState = AC_WAIT_ADMISSION;

		trafficAdmCtrl_buildTSPec(pQosMngr->pTrafficAdmCtrl, pCandidateParams, pQosIe, &ieLen);
		return ieLen;
	}
	else
	{
		return 0;
	}
}

/************************************************************************
 *                        setWmeSiteParams        			            *
 ************************************************************************
DESCRIPTION: The function is called upon association response to set QOS_WME site 
             parameters.
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.
            pQosIeParams         -  pointer to received IE parameters received 
			                        in association response. 
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static TI_STATUS setWmeSiteParams(qosMngr_t *pQosMngr, TI_UINT8 *pQosIeParams)
{
	dot11_WME_PARAM_t  *pWMEparams = (dot11_WME_PARAM_t *)pQosIeParams;
	TI_STATUS           status;         
	TI_UINT8               acID;

	if (pQosIeParams == NULL)
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "setWmeSiteParams: pQosIeParams is NULL !");
		return TI_NOK;
	}
	
	for(acID = FIRST_AC_INDEX;acID < MAX_NUM_OF_AC; acID++)
	{
	   /* configure Ack-Policy to TxCtrl. */
		txCtrlParams_setAcAckPolicy(pQosMngr->hTxCtrl, acID, pQosMngr->acParams[acID].wmeAcAckPolicy);
	}

	/* update AC params */
	pWMEparams->WME_ACParameteres.ACBEParametersRecord.TXOPLimit = 
		ENDIAN_HANDLE_WORD(pWMEparams->WME_ACParameteres.ACBEParametersRecord.TXOPLimit);
	pWMEparams->WME_ACParameteres.ACBKParametersRecord.TXOPLimit = 
		ENDIAN_HANDLE_WORD(pWMEparams->WME_ACParameteres.ACBKParametersRecord.TXOPLimit);
	pWMEparams->WME_ACParameteres.ACVIParametersRecord.TXOPLimit = 
		ENDIAN_HANDLE_WORD(pWMEparams->WME_ACParameteres.ACVIParametersRecord.TXOPLimit);
	pWMEparams->WME_ACParameteres.ACVOParametersRecord.TXOPLimit = 
		ENDIAN_HANDLE_WORD(pWMEparams->WME_ACParameteres.ACVOParametersRecord.TXOPLimit);

	status = updateACParams(pQosMngr,&(pWMEparams->WME_ACParameteres));
	if(status != TI_OK)
		return status;

	/* update Tx header convert mode */
	txCtrlParams_setQosHeaderConverMode(pQosMngr->hTxCtrl, HDR_CONVERT_QOS);

	return TI_OK;
}


/************************************************************************
 *                        updateACParams     			                *
 ************************************************************************
DESCRIPTION: the function is called upon QOS protocol updates paramters 
             to TNET and TxCtrl object 
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static TI_STATUS updateACParams(qosMngr_t *pQosMngr,dot11_ACParameters_t *pAcParams)
{
	TI_UINT8    acID,i;
	TI_STATUS   status;
    dot11_QOS_AC_IE_ParametersRecord_t *pACParameteresRecord;
    ETrafficAdmState *pAcTrafficAdmState;


	/*
	 * For QOS_WME: setting ac traffic params (edcf etc')  
	 * in this order BE, BK , VI, VO .
	 */

    pACParameteresRecord = (dot11_QOS_AC_IE_ParametersRecord_t *)pAcParams;

	for(i = FIRST_AC_INDEX; i < MAX_NUM_OF_AC; i++, pACParameteresRecord++)
	{
        acID = (pACParameteresRecord->ACI_AIFSN & AC_PARAMS_ACI_MASK) >> 5;

        pAcTrafficAdmState = &(pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState);		

		/* edcf params */

		pQosMngr->acParams[acID].acQosParams.ac        				   = acID;
		pQosMngr->acParams[acID].acQosParams.aifsn                     = pACParameteresRecord->ACI_AIFSN & AC_PARAMS_AIFSN_MASK;
		pQosMngr->acParams[acID].acQosParams.txopLimit                 = pACParameteresRecord->TXOPLimit;

        /* to use user setting ? */
        if(TI_TRUE ==pQosMngr->bCwFromUserEnable)
        {
            pQosMngr->acParams[acID].acQosParams.cwMin                 = pQosMngr->uDesireCwMin;
            pQosMngr->acParams[acID].acQosParams.cwMax                 = pQosMngr->uDesireCwMax;
        }
        else
        {
            pQosMngr->acParams[acID].acQosParams.cwMin                 = pACParameteresRecord->ECWmin_ECWmax & AC_PARAMS_CWMIN_MASK;
            pQosMngr->acParams[acID].acQosParams.cwMax                 = (pACParameteresRecord->ECWmin_ECWmax & AC_PARAMS_CWMAX_MASK) >> 4;
        }

		status = verifyAndConfigQosParams(pQosMngr,&(pQosMngr->acParams[acID].acQosParams));
		if(status != TI_OK)
			return status;


		/* UPSD configuration */
		pQosMngr->acParams[acID].QtrafficParams.psScheme = pQosMngr->acParams[acID].currentWmeAcPsMode;
		status = verifyAndConfigTrafficParams(pQosMngr,&(pQosMngr->acParams[acID].QtrafficParams));
		if(status != TI_OK)
			return status;


		/* update admission state */
		if(pACParameteresRecord->ACI_AIFSN & AC_PARAMS_ACM_MASK)
		{
			pQosMngr->acParams[acID].apInitAdmissionState = ADMISSION_REQUIRED;
			*pAcTrafficAdmState = AC_NOT_ADMITTED;

			txCtrlParams_setAcAdmissionStatus(pQosMngr->hTxCtrl, acID, ADMISSION_REQUIRED, AC_NOT_ADMITTED);
		}
		else
		{
			pQosMngr->acParams[acID].apInitAdmissionState = ADMISSION_NOT_REQUIRED;
			*pAcTrafficAdmState = AC_ADMITTED;

			txCtrlParams_setAcAdmissionStatus(pQosMngr->hTxCtrl, acID, ADMISSION_NOT_REQUIRED, AC_ADMITTED);
		}

        /* If AC is admidtted and has enabled PS-Rx-Streamings, configure it to FW */
        /* Note: this may occur after roaming */
        if (*pAcTrafficAdmState == AC_ADMITTED) 
        {
            TI_UINT32       uTid1       = WMEQosAcToTid[acID];
            TI_UINT32       uTid2       = WMEQosMateTid[uTid1];
            TPsRxStreaming *pTid1Params = &pQosMngr->aTidPsRxStreaming[uTid1];
            TPsRxStreaming *pTid2Params = &pQosMngr->aTidPsRxStreaming[uTid2];

            if (pTid1Params->bEnabled) 
            {
                TWD_CfgPsRxStreaming (pQosMngr->hTWD, pTid1Params, NULL, NULL);
            }
            if (pTid2Params->bEnabled) 
            {
                TWD_CfgPsRxStreaming (pQosMngr->hTWD, pTid2Params, NULL, NULL);
            }
        }
	}

	return TI_OK;
}



/************************************************************************
 *                        verifyWmeIeParams     			            *
 ************************************************************************
DESCRIPTION: verify QOS_WME IE.
                                                                                                   
INPUT:      hQosMngr	         -	Qos Manager handle.

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static TI_STATUS verifyWmeIeParams(qosMngr_t *pQosMngr,TI_UINT8 *pQosIeParams)
{
	dot11_WME_IE_t  WMEie;
	TI_UINT8           Len;
	dot11_WME_IE_t  *pWMERecvIe = (dot11_WME_IE_t  *)pQosIeParams;

	if(pQosIeParams == NULL)
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, ": pQosIeParams is NULL!! \n");
		return TI_NOK;
	}

	/* get QOS_WME IE */
	getWMEInfoElement(pQosMngr,(TI_UINT8 *)&WMEie,(TI_UINT8 *)&Len);

	if((WMEie.hdr[0] != pWMERecvIe->hdr[0] ) ||
	   (WMEie.OUI[0] != pWMERecvIe->OUI[0]) ||
	   (WMEie.OUI[1] != pWMERecvIe->OUI[1]) ||
	   (WMEie.OUI[2] != pWMERecvIe->OUI[2]) ||
	   (WMEie.OUIType != pWMERecvIe->OUIType))
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_WARNING, ": QosIeParams mismatch (ID or OUI)!! \n");
		return TI_NOK;
	}


    if(WMEie.version != pWMERecvIe->version)
TRACE2(pQosMngr->hReport, REPORT_SEVERITY_WARNING, ": Driver QOS_WME version: %d, Site QOS_WME version: %d\n", WMEie.version, pWMERecvIe->version);

	return TI_OK;
}


/************************************************************************
 *                        qosMngr_SetPsRxStreaming                      *
 ************************************************************************
DESCRIPTION: Verify and configure a TID PS-Rx-Streaming setting
                                                                                                   
INPUT:      pQosMngr	- Qos Manager handle.
            pNewParams  - The new TID streaming parameters to configure

OUTPUT:		

RETURN:     TI_OK on success, relevant failures otherwise

************************************************************************/
static TI_STATUS qosMngr_SetPsRxStreaming (qosMngr_t *pQosMngr, TPsRxStreaming *pNewParams)
{
    TI_UINT32       uCurrTid            = pNewParams->uTid;
    TI_UINT32       uAcId               = WMEQosTagToACTable[uCurrTid];
    TPsRxStreaming  *pCurrTidParams     = &pQosMngr->aTidPsRxStreaming[uCurrTid];
    TI_BOOL         bTidPrevEnabled     = pCurrTidParams->bEnabled;

	/* Verify STA is connected to AP */
	if (pQosMngr->isConnected == TI_FALSE)
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_SetPsRxStreaming: Not connected - Ignoring request !!!\n");
		return NOT_CONNECTED;
	}

	/* Check TID validity */
	if (uCurrTid > MAX_USER_PRIORITY)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "TID = %d > 7 !!!\n", uCurrTid);
		return PARAM_VALUE_NOT_VALID;
	}

	/* Verify that the AC is admitted */
	if (pQosMngr->resourceMgmtTable.currentTspecInfo[uAcId].trafficAdmState != AC_ADMITTED)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_SetPsRxStreaming: AC = %d is not admitted -> Ignoring request !!!\n", uAcId);
		return USER_PRIORITY_NOT_ADMITTED;
	}

	/* Verify that a disabled TID is not beeing disabled again */
	if (!pNewParams->bEnabled && !pCurrTidParams->bEnabled)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_SetPsRxStreaming: TID %d is already disabled -> Ignoring request !!!\n", uCurrTid);
		return PARAM_VALUE_NOT_VALID;
	}

	/* Verify that the max number of enabled TIDs is not exeeded */
	if (pNewParams->bEnabled  &&  
        !pCurrTidParams->bEnabled  &&
        pQosMngr->uNumEnabledPsRxStreams == MAX_ENABLED_PS_RX_STREAMS)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_SetPsRxStreaming: Can't have more than %d TIDs enabled -> Ignoring request !!!\n", MAX_ENABLED_PS_RX_STREAMS);
		return PARAM_VALUE_NOT_VALID;
	}

    /* Save the new streaming configuration of the TID */
    os_memoryCopy (pQosMngr->hOs, (void *)pCurrTidParams, (void *)pNewParams, sizeof(TPsRxStreaming));

    /* Update the relevant AC which of its TIDs parameters to use (save pointer of desired TID) */
	if (pCurrTidParams->bEnabled)
    {
        if (!bTidPrevEnabled) 
        {
            pQosMngr->uNumEnabledPsRxStreams++;
        }
    }
	else 
    {
        pQosMngr->uNumEnabledPsRxStreams--;
    }

    /* Send configuration update to the FW */
    return TWD_CfgPsRxStreaming (pQosMngr->hTWD, pCurrTidParams, NULL, NULL);
}


/************************************************************************
 *                    Admission Control Functions     		            *
 ************************************************************************/
/************************************************************************
 *                        qosMngr_requestAdmission     			        *
 ************************************************************************
DESCRIPTION: This function is API function for TSPEC request.

INPUT:      hQosMngr	         -	Qos Manager handle.
			addTspecParams		 -  The Tspec Parameters
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS qosMngr_requestAdmission(TI_HANDLE			hQosMngr, 
                                   OS_802_11_QOS_TSPEC_PARAMS *addTspecParams)
{

    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	TI_STATUS	status;
	TI_UINT8		acID;
		

	/* check if STA is already connected to AP */
	if(pQosMngr->isConnected == TI_FALSE)
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_requestAdmission: Not connected - Ignoring request !!!\n");
		return NOT_CONNECTED;
	}

	/* check if AP support QOS_WME */
	if(pQosMngr->activeProtocol != QOS_WME)
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_requestAdmission: Not connected to a QOS AP - Ignoring request !!!\n");
		return NO_QOS_AP;
	}

	/* check if Traffic Admission Control is enable */
	if(pQosMngr->trafficAdmCtrlEnable == TI_FALSE)
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_requestAdmission: Admission-Control is disabled - Ignoring request !!!\n");
		return ADM_CTRL_DISABLE;
	}

	/* check UP validity */
	if( addTspecParams->uUserPriority > MAX_USER_PRIORITY)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "uUserPriority = %d > 7 !!!\n",addTspecParams->uUserPriority);
		return TI_NOK;
	}

	/* find acID from the user priority */
	acID = WMEQosTagToACTable[addTspecParams->uUserPriority];

	/* check if signaling is already in process for this AC */
	if(pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState == AC_WAIT_ADMISSION)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_requestAdmission: AC = %d , signaling is in process -> Ignore Request !!!\n",acID);
		return TRAFIC_ADM_PENDING;
	}
	
	/* check if AC is already admitted with other UP */
	if( (pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState == AC_ADMITTED) &&
		(pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority <= MAX_USER_PRIORITY) &&
		(pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority != addTspecParams->uUserPriority) )
	{
TRACE2(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_requestAdmission: AC = %d , another UP (%d) on same AC is already admited -> Ignoring request !!!\n",			acID, pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority);
		return AC_ALREADY_IN_USE;
	}

	/* check msdu size validity */
	if( (addTspecParams->uNominalMSDUsize & (~FIXED_NOMINAL_MSDU_SIZE_MASK)) > MAX_DATA_BODY_LENGTH)
	{
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "uNominalMSDUsize = %d > 2312, !!!\n",addTspecParams->uNominalMSDUsize);
		return TI_NOK;
	}
	
	/* check PS mode validity */
	if( (addTspecParams->uAPSDFlag == PS_SCHEME_UPSD_TRIGGER) && (pQosMngr->currentPsMode != PS_SCHEME_UPSD_TRIGGER) )
	{
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "The STA's current status does not support UPSD -> Ignoring TSPEC request that has UPSD on !!!\n");
		return TI_NOK;
	}

TRACE2(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_requestAdmission: UP = %d , acID = %d\n",addTspecParams->uUserPriority, acID);

	/* set tspec parameters in candidateTspecInfo table */
	qosMngr_storeTspecCandidateParams (&(pQosMngr->resourceMgmtTable.candidateTspecInfo[acID]), 
										addTspecParams, (TI_UINT8)acID);

	/* Perhaps this should be done only if the request was successfully sent */
	if (acID == QOS_AC_VO) 
	{
		pQosMngr->voiceTspecConfigured = TI_TRUE;
	}
	
	if (acID == QOS_AC_VI) 
	{
		pQosMngr->videoTspecConfigured = TI_TRUE;
	}
	
	/* call TrafficAdmCtrl API function for the signaling proccess */
	status = trafficAdmCtrl_startAdmRequest(pQosMngr->pTrafficAdmCtrl, &(pQosMngr->resourceMgmtTable.candidateTspecInfo[acID]));

	if(status == TI_OK)
	{
		/* request transmitted TI_OK */
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState = AC_WAIT_ADMISSION;
TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_requestAdmission: UP = %d , request TI_OK !!!\n",addTspecParams->uUserPriority);
	}
	else
	{
		/* reaquest not transmitted TI_OK */
		pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;
TRACE2(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "qosMngr_requestAdmission: UP = %d , request  NOT TI_OK status=%d!!!\n",addTspecParams->uUserPriority, status);
		return TI_NOK;
	}

	return status;
}

/************************************************************************
 *                        qosMngr_deleteAdmission     		            *
 ************************************************************************
DESCRIPTION: This function is API fuunction for tspec delete.

INPUT:      hQosMngr	         -	Qos Manager handle.
			delAdmissionParams	 -  
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

TI_STATUS qosMngr_deleteAdmission(TI_HANDLE hQosMngr, OS_802_11_QOS_DELETE_TSPEC_PARAMS *delAdmissionParams)
{

    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
	TI_UINT8		acID;
	
	/* check UP validity */
	if( delAdmissionParams->uUserPriority > MAX_USER_PRIORITY )
	{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_deleteAdmission: userPriority > 7 -> Ignore !!!");
		return TI_NOK;
	}

	/* check if STA is already connected to AP */
	if(pQosMngr->isConnected == TI_FALSE)
	{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_deleteAdmission: pQosMngr->connected == TI_FALSE -> Ignore !!!");
		return NOT_CONNECTED;
	}

	/* check if AP support QOS_WME */
	if(pQosMngr->activeProtocol != QOS_WME)
	{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_deleteAdmission: activeProtocol != QOS_WME -> Ignore !!!");
		return NO_QOS_AP;
	}

	/* find acID from the user priority */
	acID = WMEQosTagToACTable[delAdmissionParams->uUserPriority];

	/* check if tspec is already addmited for this AC */
	if(pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState != AC_ADMITTED)
	{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_deleteAdmission: AC is not ADMITED -> Ignore !!!");
		return TI_NOK;
	}

	/* check if AC is already admited with the same UP */
	if(pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority != delAdmissionParams->uUserPriority)
	{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_deleteAdmission: user priority is invalid. -> Ignore !!!\n");
		return USER_PRIORITY_NOT_ADMITTED;
	}

	/* check if signaling is already in procces for this AC */
	if(pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState == AC_WAIT_ADMISSION)
	{	
TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_deleteAdmission: AC is under negotiation -> Ignore !!!");
		return TRAFIC_ADM_PENDING;
	}


	
	/* call TrafficAdmCtrl API function for the delete tspec */
	trafficAdmCtrl_sendDeltsFrame(pQosMngr->pTrafficAdmCtrl, &(pQosMngr->resourceMgmtTable.currentTspecInfo[acID]), 
										(TI_UINT8)delAdmissionParams->uReasonCode );

	
	deleteTspecConfiguration(pQosMngr, acID);
	
	return TI_OK;

}
/************************************************************************
 *                        deleteTspecConfiguration     		            *
 ************************************************************************
DESCRIPTION: configure the driver and FW to default configuration after
			 tspec deletion.

INPUT:      hQosMngr	             - Qos Manager handle.
			acID					 - the AC of the Tspec to delete
OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/

static void deleteTspecConfiguration(qosMngr_t *pQosMngr, TI_UINT8 acID)
{
	paramInfo_t param;

    /* Zero Tspec parameters */
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].nominalMsduSize = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].minimumPHYRate = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].meanDataRate = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].surplausBwAllowance = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].mediumTime = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].UPSDFlag = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].uMinimumServiceInterval = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].uMaximumServiceInterval = 0;
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].streamDirection = BI_DIRECTIONAL;
	
	/* update total medium time */
	pQosMngr->resourceMgmtTable.totalAllocatedMediumTime -= pQosMngr->resourceMgmtTable.currentTspecInfo[acID].mediumTime;
	
	/* disable TSRS for this ac */
	param.content.txDataQosParams.acID = acID;
	param.content.txDataQosParams.tsrsArrLen = 0;
	param.paramType = CTRL_DATA_TSRS_PARAM;
	ctrlData_setParam(pQosMngr->hCtrlData, &param);

	/* stop TS metrix for this ac */
#ifdef XCC_MODULE_INCLUDED
	measurementMgr_disableTsMetrics(pQosMngr->hMeasurementMngr, acID);
#endif

	/* update medium time and rate adaptation event only when init admission bit was 0 */
	if( pQosMngr->acParams[acID].apInitAdmissionState == ADMISSION_REQUIRED )
	{
		/* update currentTspecInfo parameters */
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;
		
		/* set params to TX */
		txCtrlParams_setAdmissionCtrlParams(pQosMngr->hTxCtrl,
									acID,
									pQosMngr->resourceMgmtTable.currentTspecInfo[acID].mediumTime , 
									pQosMngr->resourceMgmtTable.currentTspecInfo[acID].minimumPHYRate, TI_FALSE);
	}
	else
	{
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState = AC_ADMITTED;
	}

    /* After we have updated the TxCtrl with the new status of the UP, we can zero the userPriority field */
    pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority = INACTIVE_USER_PRIORITY;

	/* set PS mode according to the PS mode from the association */
    /* restore the current Ps mode per AC to UPSD ONLY IF both the station and AP support UPSD */
    if ((pQosMngr->currentPsMode == PS_SCHEME_UPSD_TRIGGER) && (pQosMngr->acParams[acID].desiredWmeAcPsMode == PS_SCHEME_UPSD_TRIGGER))
    {
	  pQosMngr->acParams[acID].currentWmeAcPsMode = PS_SCHEME_UPSD_TRIGGER;
    }
    else
    {
	  pQosMngr->acParams[acID].currentWmeAcPsMode = PS_SCHEME_LEGACY;
    }

	if(acID == QOS_AC_VO)
	{
		pQosMngr->voiceTspecConfigured = TI_FALSE;
	}
	
	if (acID == QOS_AC_VI) 
	{
		pQosMngr->videoTspecConfigured = TI_FALSE;
    }

	/* UPSD_FW - open comment in UPSD FW integration */
	 
	/* UPSD configuration */
	pQosMngr->acParams[acID].QtrafficParams.psScheme = pQosMngr->acParams[acID].currentWmeAcPsMode;
	verifyAndConfigTrafficParams(pQosMngr,&(pQosMngr->acParams[acID].QtrafficParams));

    /* If the AC is not admitted, disable its TIDs' PS-Streamings if enabled */
    if (pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState == AC_NOT_ADMITTED)
    {
        TI_UINT32       uTid1       = WMEQosAcToTid[acID];
        TI_UINT32       uTid2       = WMEQosMateTid[uTid1];
        TPsRxStreaming *pTid1Params = &pQosMngr->aTidPsRxStreaming[uTid1];
        TPsRxStreaming *pTid2Params = &pQosMngr->aTidPsRxStreaming[uTid2];

        if (pTid1Params->bEnabled) 
        {
            pTid1Params->bEnabled = TI_FALSE;
            TWD_CfgPsRxStreaming (pQosMngr->hTWD, pTid1Params, NULL, NULL);
            pQosMngr->uNumEnabledPsRxStreams--;
        }
        if (pTid2Params->bEnabled) 
        {
            pTid2Params->bEnabled = TI_FALSE;
            TWD_CfgPsRxStreaming (pQosMngr->hTWD, pTid2Params, NULL, NULL);
            pQosMngr->uNumEnabledPsRxStreams--;
        }
    }
}

/*-----------------------------------------------------------------------------
Routine Name: qosMngr_sendUnexpectedTSPECResponse
Routine Description: send event to user application, informing of unexpected TSPEC response
					 which might imply loss of UPSD mode synch between AP and STA
Arguments: pTspecInfo - contains unexpected TSPEC response information 
Return Value:
-----------------------------------------------------------------------------*/
TI_STATUS qosMngr_sendUnexpectedTSPECResponseEvent(TI_HANDLE	hQosMngr,
								   tspecInfo_t	*pTspecInfo)
{
	OS_802_11_QOS_TSPEC_PARAMS addtsReasonCode;
    qosMngr_t *pQosMngr =	(qosMngr_t *)hQosMngr;

	/* set the event params */
	addtsReasonCode.uAPSDFlag = pTspecInfo->UPSDFlag;
	addtsReasonCode.uMinimumServiceInterval = pTspecInfo->uMinimumServiceInterval;
	addtsReasonCode.uMaximumServiceInterval = pTspecInfo->uMaximumServiceInterval;
	addtsReasonCode.uUserPriority = pTspecInfo->userPriority;
	addtsReasonCode.uNominalMSDUsize = pTspecInfo->nominalMsduSize;
	addtsReasonCode.uMeanDataRate = pTspecInfo->meanDataRate;	
	addtsReasonCode.uMinimumPHYRate = pTspecInfo->minimumPHYRate;
	addtsReasonCode.uSurplusBandwidthAllowance = pTspecInfo->surplausBwAllowance;
	addtsReasonCode.uMediumTime = pTspecInfo->mediumTime;

    addtsReasonCode.uReasonCode = pTspecInfo->statusCode + TSPEC_RESPONSE_UNEXPECTED;
		
	/* send event */
	EvHandlerSendEvent(pQosMngr->hEvHandler, IPC_EVENT_TSPEC_STATUS, (TI_UINT8*)(&addtsReasonCode), sizeof(OS_802_11_QOS_TSPEC_PARAMS));

	return TI_OK;
}

/************************************************************************
 *                        qosMngr_setAdmissionInfo                      *
 ************************************************************************
DESCRIPTION: This function is API function.
            the trafficAdmCtrl object calls this function in
            order to update the QOSMngr on TSPEC request status

INPUT:      hQosMngr                 - Qos Manager handle.
            pTspecInfo               - The TSPEC Parameters
            trafficAdmRequestStatus  - the status of the request
OUTPUT:     

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS qosMngr_setAdmissionInfo(TI_HANDLE    hQosMngr, 
                                   TI_UINT8        acID,
                                   tspecInfo_t  *pTspecInfo,
                                   trafficAdmRequestStatus_e trafficAdmRequestStatus)
{
    TI_UINT32                 actualMediumTime;
    OS_802_11_QOS_TSPEC_PARAMS addtsReasonCode;
    qosMngr_t *pQosMngr =  (qosMngr_t *)hQosMngr;
    TSetTemplate           templateStruct;
    QosNullDataTemplate_t  QosNullDataTemplate;

    /* Check if the updated AC is in WAIT state */
    if(pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState != AC_WAIT_ADMISSION)
    {
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setAdmissionInfo: acID = %d, trafficAdmState != WAIT. IGNORE !!!\n", acID);
        
        return TI_NOK;
    }

    if (pQosMngr->TSPECNegotiationResultCallb != NULL)
    {
        pQosMngr->TSPECNegotiationResultCallb (pQosMngr->TSPECNegotiationResultModule, trafficAdmRequestStatus);
        pQosMngr->TSPECNegotiationResultCallb = NULL;
        pQosMngr->TSPECNegotiationResultModule = NULL;
    }

    switch(trafficAdmRequestStatus)
    {
    case STATUS_TRAFFIC_ADM_REQUEST_ACCEPT:
        /* Received admission response with status accept */

        TRACE3(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_setAdmissionInfo: admCtrl status =  REQUEST_ACCEPT [ acID = %d, mediumTime = %d, minimumPHYRate = %d ]\n", acID, pTspecInfo->mediumTime, pTspecInfo->minimumPHYRate);
        
        /* Set the event params */
        addtsReasonCode.uAPSDFlag = pTspecInfo->UPSDFlag;
        addtsReasonCode.uMinimumServiceInterval = pTspecInfo->uMinimumServiceInterval;
        addtsReasonCode.uMaximumServiceInterval = pTspecInfo->uMaximumServiceInterval;
        addtsReasonCode.uUserPriority = pTspecInfo->userPriority;
        addtsReasonCode.uNominalMSDUsize = pTspecInfo->nominalMsduSize & ~FIXED_NOMINAL_MSDU_SIZE_MASK;
        addtsReasonCode.uMeanDataRate = pTspecInfo->meanDataRate;   
        addtsReasonCode.uMinimumPHYRate = pTspecInfo->minimumPHYRate;
        addtsReasonCode.uSurplusBandwidthAllowance = pTspecInfo->surplausBwAllowance;
        addtsReasonCode.uMediumTime = pTspecInfo->mediumTime;

        /* Free the candidate parameters */
        pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;

        /* Validate tid matching */
        if (pTspecInfo->tid == pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].tid)
        {
            addtsReasonCode.uReasonCode = ADDTS_RESPONSE_ACCEPT;
            
            /* Send event */
            EvHandlerSendEvent (pQosMngr->hEvHandler, 
                                IPC_EVENT_TSPEC_STATUS, 
                                (TI_UINT8*)&addtsReasonCode, 
                                sizeof(OS_802_11_QOS_TSPEC_PARAMS));
        }
        else
        {
            addtsReasonCode.uReasonCode = ADDTS_RESPONSE_AP_PARAM_INVALID;
            
            /* Send event */
            EvHandlerSendEvent (pQosMngr->hEvHandler, 
                                IPC_EVENT_TSPEC_STATUS, 
                                (TI_UINT8*)&addtsReasonCode, 
                                sizeof(OS_802_11_QOS_TSPEC_PARAMS));
            return TI_OK;
        }

        /* Update the current TSPEC parameters from the received TSPEC */
        os_memoryCopy (pQosMngr->hOs, 
                       &pQosMngr->resourceMgmtTable.currentTspecInfo[acID], 
                       pTspecInfo, 
                       sizeof(tspecInfo_t));

        /* Set the TSPEC to admitted */
        pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState = AC_ADMITTED;
        
        /* Update total medium time */
        pQosMngr->resourceMgmtTable.totalAllocatedMediumTime += pTspecInfo->mediumTime;

        /*
         * Set QOS Null-data template into the firmware. 
         * When a new TSPEC with UPSD is "accepted" by the AP, 
         * we set the user priority of it into the firmware. 
         * Since this AC is already ADMITTED (we are processing the successful response), 
         * it is TI_OK to set the qos null data template with this UP 
         */
        if (addtsReasonCode.uAPSDFlag == PS_SCHEME_UPSD_TRIGGER && 
            pQosMngr->QosNullDataTemplateUserPriority == 0xFF)
        {
            /* Remember the user priority which we have set */
            pQosMngr->QosNullDataTemplateUserPriority = (TI_UINT8)addtsReasonCode.uUserPriority;

            templateStruct.ptr = (TI_UINT8 *)&QosNullDataTemplate;
            templateStruct.type = QOS_NULL_DATA_TEMPLATE;
            templateStruct.uRateMask = RATE_MASK_UNSPECIFIED;
            buildQosNullDataTemplate (pQosMngr->hSiteMgr, &templateStruct, pQosMngr->QosNullDataTemplateUserPriority);
            TWD_CmdTemplate (pQosMngr->hTWD, &templateStruct, NULL, NULL);

            TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_setAdmissionInfo: Setting QOS null data for UserPriority=%d (due to TSPEC ACCEPT response)\n", addtsReasonCode.uUserPriority);
        }

        /* Set params to TX */
        /*------------------*/

        /* Update medium time and rate adaptation event only when init admission bit was 0 */
        if (pQosMngr->acParams[acID].apInitAdmissionState == ADMISSION_REQUIRED)
        {
            /* mediumTime is in units of 32uSec and we work in mSec */
            actualMediumTime = (pTspecInfo->mediumTime * 32) / 1000;

            /* Set TX params */
			txCtrlParams_setAdmissionCtrlParams(pQosMngr->hTxCtrl,
                                          acID,
                                          actualMediumTime, 
                                          pTspecInfo->minimumPHYRate, 
                                          TI_TRUE);
        }
        
        {
            PSScheme_e psMode = pTspecInfo->UPSDFlag ? PS_SCHEME_UPSD_TRIGGER 
                                                     : PS_SCHEME_LEGACY; 
       
            if (pQosMngr->acParams[acID].currentWmeAcPsMode != psMode)
            {
                TI_STATUS status;

                pQosMngr->acParams[acID].currentWmeAcPsMode = psMode;

                /* UPSD_FW - open comment in UPSD FW integration */
                pQosMngr->acParams[acID].QtrafficParams.psScheme = pQosMngr->acParams[acID].currentWmeAcPsMode;
                status = verifyAndConfigTrafficParams (pQosMngr, &pQosMngr->acParams[acID].QtrafficParams);
                if (status != TI_OK)
                    return status;
            }
        }   
        break;

    case STATUS_TRAFFIC_ADM_REQUEST_REJECT:
        /* Received admission response with status reject */

        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_setAdmissionInfo: admCtrl status = REQUEST_REJECT [ acID = %d ]\n", acID);
        
        /* Validate tid matching */
        if (pTspecInfo->tid == pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].tid)
        {
            addtsReasonCode.uReasonCode = pTspecInfo->statusCode;
        }
        else
        {
            addtsReasonCode.uReasonCode = ADDTS_RESPONSE_AP_PARAM_INVALID;
        }

        /* Free the candidate parameters */
        pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;

        /* Send event to application */
        addtsReasonCode.uAPSDFlag = pTspecInfo->UPSDFlag;
        addtsReasonCode.uMinimumServiceInterval = pTspecInfo->uMinimumServiceInterval;
        addtsReasonCode.uMaximumServiceInterval = pTspecInfo->uMaximumServiceInterval;
        addtsReasonCode.uUserPriority = pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].userPriority;
        addtsReasonCode.uNominalMSDUsize = pTspecInfo->nominalMsduSize & ~FIXED_NOMINAL_MSDU_SIZE_MASK;
        addtsReasonCode.uMeanDataRate = pTspecInfo->meanDataRate;   
        addtsReasonCode.uMinimumPHYRate = pTspecInfo->minimumPHYRate;
        addtsReasonCode.uSurplusBandwidthAllowance = pTspecInfo->surplausBwAllowance;
        addtsReasonCode.uMediumTime = pTspecInfo->mediumTime;
    
        EvHandlerSendEvent (pQosMngr->hEvHandler, 
                            IPC_EVENT_TSPEC_STATUS, 
                            (TI_UINT8*)&addtsReasonCode, 
                            sizeof(OS_802_11_QOS_TSPEC_PARAMS));       
        break;

    case STATUS_TRAFFIC_ADM_REQUEST_TIMEOUT:
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "qosMngr_setAdmissionInfo: admCtrl status = REQUEST_TIMEOUT [ acID = %d ]\n", acID);
        
        /* Free the candidate parameters */
        pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;

        /* Send event to application */
        addtsReasonCode.uUserPriority = pQosMngr->resourceMgmtTable.candidateTspecInfo[acID].userPriority;
        addtsReasonCode.uReasonCode = ADDTS_RESPONSE_TIMEOUT;
        addtsReasonCode.uAPSDFlag = 0;
        addtsReasonCode.uMinimumServiceInterval = 0;
        addtsReasonCode.uMaximumServiceInterval = 0;
        addtsReasonCode.uNominalMSDUsize = 0;
        addtsReasonCode.uMeanDataRate = 0;  
        addtsReasonCode.uMinimumPHYRate = 0;
        addtsReasonCode.uSurplusBandwidthAllowance = 0;
        addtsReasonCode.uMediumTime = 0;

        EvHandlerSendEvent (pQosMngr->hEvHandler, 
                            IPC_EVENT_TSPEC_STATUS, 
                            (TI_UINT8*)&addtsReasonCode, 
                            sizeof(OS_802_11_QOS_TSPEC_PARAMS));       
        break;
       
    default:
        TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_setAdmissionInfo: receive state from admCtrl = unknown !!! \n");
        break;
    }

    return TI_OK;  
}

/************************************************************************
 *                    QosMngr_receiveActionFrames                       *
 ************************************************************************
DESCRIPTION: 
                                                                                                 
RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
TI_STATUS QosMngr_receiveActionFrames(TI_HANDLE hQosMngr, TI_UINT8* pData, TI_UINT8 action, TI_UINT32 bodyLen)
{
	TI_UINT8					    acID;
	tsInfo_t				        tsInfo;
	TI_UINT8					    userPriority;
    OS_802_11_QOS_TSPEC_PARAMS      addtsReasonCode;
    TI_UINT8                        statusCode = 0;
#ifdef XCC_MODULE_INCLUDED
	paramInfo_t                     param;
#endif
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	/* check if STA is already connected to AP */
	if( (pQosMngr->isConnected == TI_FALSE) || 
		(pQosMngr->activeProtocol != QOS_WME) || 
		(pQosMngr->trafficAdmCtrlEnable == TI_FALSE) )
	{
        TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "QosMngr_receiveActionFrames:  Ignore  !!!");
		return TI_NOK;
	}

	/* check DELTS action code */
	if (action == DELTS_ACTION) 
	{
		/* 
		 *  parse the frame 
		 */

		/* skip dialog-token (1 byte), status-code (1 byte) and dot11_WME_TSPEC_IE header (8 bytes). */
		pData += 10;	  

		/*  Get TS-Info from TSpec IE in DELTS, and get from it the user-priority. */
		tsInfo.tsInfoArr[0] = *pData;
		pData++;
        tsInfo.tsInfoArr[1] = *pData;
		pData++;
		tsInfo.tsInfoArr[2] = *pData;
		
        userPriority = (((tsInfo.tsInfoArr[1]) & TS_INFO_1_USER_PRIORITY_MASK) >> USER_PRIORITY_SHIFT);
		
		acID = WMEQosTagToACTable[userPriority];
		

        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_INFORMATION, "QosMngr_receiveActionFrames: DELTS [ acID = %d ] \n", acID);


		/* check if this AC is admitted with the correct userPriority */
		if( (pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState == AC_ADMITTED) &&
			( pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority == userPriority) )
		{
			deleteTspecConfiguration(pQosMngr, acID);

            /* Send event to notify DEL_TS */
            addtsReasonCode.uAPSDFlag = 0;
            addtsReasonCode.uMinimumServiceInterval = 0;
            addtsReasonCode.uMaximumServiceInterval = 0;
		    addtsReasonCode.uUserPriority = userPriority;
            addtsReasonCode.uReasonCode = TSPEC_DELETED_BY_AP;
		    addtsReasonCode.uNominalMSDUsize = 0;
		    addtsReasonCode.uMeanDataRate = 0;	
		    addtsReasonCode.uMinimumPHYRate = 0;
		    addtsReasonCode.uSurplusBandwidthAllowance = 0;
		    addtsReasonCode.uMediumTime = 0;

            EvHandlerSendEvent(pQosMngr->hEvHandler, IPC_EVENT_TSPEC_STATUS, (TI_UINT8*)(&addtsReasonCode), sizeof(OS_802_11_QOS_TSPEC_PARAMS));
		}
		else
		{
            TRACE3(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "QosMngr_receiveActionFrames: DELTS [ acID = %d userPriority = %d  currentUserPriority = %d] Current State in not ADMITED !! \n", acID, userPriority,pQosMngr->resourceMgmtTable.currentTspecInfo[acID].userPriority);
			
		}
	}
	/* if action code is ADDTS call trafficAdmCtrl object API function */
	else if (action == ADDTS_RESPONSE_ACTION) 
	{
        statusCode = *(pData+1);

#ifdef XCC_MODULE_INCLUDED

        if (ADDTS_STATUS_CODE_SUCCESS != statusCode)
        {
            tspecInfo_t tspecInfo;
            param.paramType = ROAMING_MNGR_TRIGGER_EVENT;
            param.content.roamingTriggerType =  ROAMING_TRIGGER_TSPEC_REJECTED;
            roamingMngr_setParam(pQosMngr->hRoamMng, &param);

                       
            trafficAdmCtrl_parseTspecIE(&tspecInfo, pData+2);
            
            addtsReasonCode.uAPSDFlag = tspecInfo.UPSDFlag;
            addtsReasonCode.uMinimumServiceInterval = tspecInfo.uMinimumServiceInterval;
            addtsReasonCode.uMaximumServiceInterval = tspecInfo.uMaximumServiceInterval;
            addtsReasonCode.uUserPriority = (TI_UINT32)tspecInfo.userPriority;
            addtsReasonCode.uReasonCode = ADDTS_RESPONSE_REJECT;
            addtsReasonCode.uNominalMSDUsize = (TI_UINT32)tspecInfo.nominalMsduSize & ~FIXED_NOMINAL_MSDU_SIZE_MASK;
            addtsReasonCode.uMeanDataRate = tspecInfo.meanDataRate;	
            addtsReasonCode.uMinimumPHYRate = tspecInfo.minimumPHYRate;
            addtsReasonCode.uSurplusBandwidthAllowance = (TI_UINT32)tspecInfo.surplausBwAllowance;
            addtsReasonCode.uMediumTime = (TI_UINT32)tspecInfo.mediumTime;
            
            EvHandlerSendEvent(pQosMngr->hEvHandler, IPC_EVENT_TSPEC_STATUS, (TI_UINT8*)(&addtsReasonCode), sizeof(OS_802_11_QOS_TSPEC_PARAMS));
        }
#endif
        
		if (trafficAdmCtrl_recv(pQosMngr->pTrafficAdmCtrl, pData, action) == TI_OK)
		{
#ifdef XCC_MODULE_INCLUDED
			/* Check if XCC IEs present, if so, parse them and update relevant modules; 
               skip the TSPEC IE;
               do not forget 2 bytes of status and dialog code that must be skipped as well */
			XCCv4IEs_t			XCCIE;
			TI_UINT32 				readLen;

			XCCIE.edcaLifetimeParameter = NULL;
			XCCIE.trafficStreamParameter = NULL;
			XCCIE.tsMetrixParameter = NULL;

			userPriority = GET_USER_PRIORITY_FROM_WME_TSPEC_IE(pData+2);
			acID = WMEQosTagToACTable[userPriority];

			/* The length is in the second byte of the IE header, after the token and status. */
			readLen = (TI_UINT32)(*(pData + 3)); 

			/* 4 stands for 1 byte of token + 1 byte of status + 1 byte of EID + 1 byte of len */
			bodyLen = bodyLen - 4 - readLen; 
			pData = pData + 4 + readLen;

			while (bodyLen) 
			{
				mlmeParser_readXCCOui(pData, bodyLen, &readLen, &XCCIE);
				bodyLen -= readLen;
				pData += readLen;
			}

			XCCMngr_setXCCQoSParams(pQosMngr->hXCCMgr, &XCCIE, acID);
#endif
		}
	}
	else
	{
        TRACE1(pQosMngr->hReport, REPORT_SEVERITY_WARNING, "QosMngr_receiveActionFrames: Receive unknown action code = %d  -> Ignore !! \n",action);
	}
	
	return TI_OK;
}

/************************************************************************
 *                        qosMngr_getCurrAcStatus     		            *
 ************************************************************************
DESCRIPTION: This function is API fuunction for getting tha AC status .

INPUT:      hQosMngr	             - Qos Manager handle.
			pAcStatusParams

OUTPUT:		

RETURN:     TI_OK on success, TI_NOK otherwise

************************************************************************/
static TI_STATUS qosMngr_getCurrAcStatus(TI_HANDLE hQosMngr, OS_802_11_AC_UPSD_STATUS_PARAMS *pAcStatusParams)
{
    qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;

	/* check AC validity */
	if( pAcStatusParams->uAC > MAX_NUM_OF_AC - 1 )
	{	
		TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getCurrAcStatus: acID > 3 -> Ignore !!!");
		return TI_NOK;
	}

	/* check if sta is connected to AP */
	if(pQosMngr->isConnected == TI_FALSE)
	{	
		TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getCurrAcStatus: pQosMngr->connected == TI_FALSE -> Ignore !!!");
		return NOT_CONNECTED;
	}
	
	 /* check if AP support QOS_WME */
	if(pQosMngr->activeProtocol != QOS_WME)
	{	
		TRACE0(pQosMngr->hReport, REPORT_SEVERITY_ERROR, "qosMngr_getCurrAcStatus: activeProtocol != QOS_WME -> Ignore !!!");
		return NO_QOS_AP;
	}

	pAcStatusParams->uCurrentUAPSDStatus = pQosMngr->acParams[pAcStatusParams->uAC].currentWmeAcPsMode;
	pAcStatusParams->pCurrentAdmissionStatus = pQosMngr->resourceMgmtTable.currentTspecInfo[pAcStatusParams->uAC].trafficAdmState;

	return TI_OK;
}



/************************************************************************
 *                        setNonQosAdmissionState  		                *
 ************************************************************************
DESCRIPTION: This function resets the admission state variables as required
				for non-QoS mode and configures the Tx module.

INPUT:      pQosMngr	- Qos Manager pointer.
			acId		- the AC to update.

OUTPUT:		

RETURN:     

************************************************************************/

static void setNonQosAdmissionState(qosMngr_t *pQosMngr, TI_UINT8 acID)
{
	if(acID == QOS_AC_BE)
	{
		pQosMngr->acParams[acID].apInitAdmissionState = ADMISSION_NOT_REQUIRED;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState = AC_ADMITTED;
		
		txCtrlParams_setAcAdmissionStatus(pQosMngr->hTxCtrl, acID, ADMISSION_NOT_REQUIRED, AC_ADMITTED);
	}
	else
	{
		pQosMngr->acParams[acID].apInitAdmissionState = ADMISSION_REQUIRED;
		pQosMngr->resourceMgmtTable.currentTspecInfo[acID].trafficAdmState = AC_NOT_ADMITTED;

		txCtrlParams_setAcAdmissionStatus(pQosMngr->hTxCtrl, acID, ADMISSION_REQUIRED, AC_NOT_ADMITTED);
	}
}

static void qosMngr_storeTspecCandidateParams (tspecInfo_t *pCandidateParams, OS_802_11_QOS_TSPEC_PARAMS *pTSPECParams, TI_UINT8 ac)
{
	pCandidateParams->AC = (EAcTrfcType)ac;
	pCandidateParams->tid = (TI_UINT8)pTSPECParams->uUserPriority;
	pCandidateParams->userPriority = (TI_UINT8)pTSPECParams->uUserPriority;
	pCandidateParams->meanDataRate = pTSPECParams->uMeanDataRate;
	pCandidateParams->nominalMsduSize = ((TI_UINT16)pTSPECParams->uNominalMSDUsize) | FIXED_NOMINAL_MSDU_SIZE_MASK;
	pCandidateParams->UPSDFlag = (TI_BOOL)pTSPECParams->uAPSDFlag;
	pCandidateParams->uMinimumServiceInterval = pTSPECParams->uMinimumServiceInterval;
	pCandidateParams->uMaximumServiceInterval = pTSPECParams->uMaximumServiceInterval;
	pCandidateParams->surplausBwAllowance = (TI_UINT16)pTSPECParams->uSurplusBandwidthAllowance;
	pCandidateParams->minimumPHYRate = pTSPECParams->uMinimumPHYRate;
	pCandidateParams->streamDirection = BI_DIRECTIONAL;
	pCandidateParams->mediumTime = 0;
}

