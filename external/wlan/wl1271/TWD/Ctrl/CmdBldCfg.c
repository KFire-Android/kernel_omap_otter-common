/*
 * CmdBldCfg.c
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


/** \file  CmdBldCfg.c 
 *  \brief Command builder. Configuration commands
 *
 *  \see   CmdBld.h 
 */
#define __FILE_ID__  FILE_ID_91
#include "osApi.h"
#include "tidef.h"
#include "report.h"
#include "CmdBld.h"
#include "CmdBldCfgIE.h"
#include "TWDriverInternal.h"


/****************************************************************************
 *                      cmdBld_CfgRx()
 ****************************************************************************
 * DESCRIPTION: Sets the filters according to the given configuration. 
 * 
 * INPUTS:  RxConfigOption  - The given Rx filters configuration
 *          RxFilterOption  - The given Rx filters options
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgRx (TI_HANDLE hCmdBld, TI_UINT32 uRxConfigOption, TI_UINT32 uRxFilterOption, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).RxConfigOption = uRxConfigOption;
    DB_WLAN(hCmdBld).RxFilterOption = uRxFilterOption;
    DB_WLAN(hCmdBld).RxConfigOption |= RX_CFG_ENABLE_PHY_HEADER_PLCP;
  #if defined (TNETW_MASTER_MODE) || defined (TNETW_USB_MODE)
    DB_WLAN(hCmdBld).RxConfigOption |= RX_CFG_COPY_RX_STATUS;
  #endif    

    if (DB_WLAN(hCmdBld).RxDisableBroadcast)
    {
        DB_WLAN(hCmdBld).RxConfigOption |= RX_CFG_DISABLE_BCAST;
    }

    return cmdBld_CfgIeRx (hCmdBld,
                           DB_WLAN(hCmdBld).RxConfigOption,
                           DB_WLAN(hCmdBld).RxFilterOption,
                           fCb,
                           hCb);
}


/****************************************************************************
 *                      cmdBld_CfgArpIpAddrTable()
 ****************************************************************************
 * DESCRIPTION: Sets the ARP IP table according to the given configuration. 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgArpIpAddrTable (TI_HANDLE hCmdBld, TIpAddr tIpAddr, TI_UINT8 bEnabled, EIpVer eIpVer, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).arp_IP_ver = eIpVer;

    /* no support for IPV6 */
    if (eIpVer == IP_VER_4) 
    {
        IP_COPY (DB_WLAN(hCmdBld).arp_IP_addr, tIpAddr);
    }

    DB_WLAN(hCmdBld).arpFilterType = (EArpFilterType)bEnabled;

    /* Set the new ip with the current state (e/d) */
    return cmdBld_CfgIeArpIpFilter (hCmdBld, tIpAddr, (EArpFilterType)bEnabled, fCb, hCb);
}

 /****************************************************************************
 *                      cmdBld_CfgArpIpFilter()
 ****************************************************************************
 * DESCRIPTION: Enable\Disable the ARP filter 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgArpIpFilter (TI_HANDLE hCmdBld, TIpAddr tIpAddr, void *fCb, TI_HANDLE hCb) 
{
    /* no support for IPV6 */
    IP_COPY (DB_WLAN(hCmdBld).arp_IP_addr, tIpAddr);

    return cmdBld_CfgIeArpIpFilter (hCmdBld, 
                                    tIpAddr,
                                    DB_WLAN(hCmdBld).arpFilterType,
                                    fCb,
                                    hCb);
}
/****************************************************************************
 *                      cmdBld_CfgGroupAddressTable()
 ****************************************************************************
 * DESCRIPTION: Sets the Group table according to the given configuration. 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgGroupAddressTable (TI_HANDLE        hCmdBld,
                                       TI_UINT8         uNumGroupAddr, 
                                       TMacAddr         *pGroupAddr,
                                       TI_BOOL          bEnabled, 
                                       void             *fCb, 
                                       TI_HANDLE        hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32    i;

    if (uNumGroupAddr > MAX_MULTICAST_GROUP_ADDRS) 
    {
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_CfgGroupAddressTable: numGroupAddrs=%d\n", uNumGroupAddr);
        return PARAM_VALUE_NOT_VALID;
    }

    if (NULL == pGroupAddr)
    {
        TRACE2(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_CfgGroupAddressTable: numGroupAddrs=%d Group_addr=0x%x  !!!\n", uNumGroupAddr, pGroupAddr);
        return PARAM_VALUE_NOT_VALID;
    }

    /* Keeps the parameters in the db */
    DB_WLAN(hCmdBld).numGroupAddrs = uNumGroupAddr;
    DB_WLAN(hCmdBld).isMacAddrFilteringnabled = bEnabled;

    for (i = 0; i < uNumGroupAddr; i++) 
    {
        MAC_COPY (DB_WLAN(hCmdBld).aGroupAddr[i], *(pGroupAddr + i));
    }

    return cmdBld_CfgIeGroupAdressTable (hCmdBld, uNumGroupAddr, pGroupAddr, bEnabled, fCb, hCb);  
}


/****************************************************************************
 *                      cmdBld_CfgRtsThreshold()
 ****************************************************************************
 * DESCRIPTION: Sets the Rts Threshold. 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK  pWlanParams->RtsThreshold
 ****************************************************************************/
TI_STATUS cmdBld_CfgRtsThreshold (TI_HANDLE hCmdBld, TI_UINT16 threshold, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeRtsThreshold (hCmdBld, threshold, fCb, hCb);
}

/****************************************************************************
 *                      cmdBld_CfgDcoItrimParams()
 ****************************************************************************
 * DESCRIPTION: Sets the DCO Itrim parameters. 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK 
 ****************************************************************************/
TI_STATUS cmdBld_CfgDcoItrimParams (TI_HANDLE hCmdBld, TI_BOOL enable, TI_UINT32 moderationTimeoutUsec, void *fCb, TI_HANDLE hCb)
{
    /* Keeps the parameters in the db */
    DB_WLAN(hCmdBld).dcoItrimEnabled = enable;
    DB_WLAN(hCmdBld).dcoItrimModerationTimeoutUsec = moderationTimeoutUsec;

    return cmdBld_CfgIeDcoItrimParams (hCmdBld, enable, moderationTimeoutUsec, fCb, hCb);
}

/****************************************************************************
 *                      cmdBld_CfgFragmentThreshold()
 ****************************************************************************
 * DESCRIPTION: Sets the tx fragmentation Threshold. 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK 
 ****************************************************************************/
TI_STATUS cmdBld_CfgFragmentThreshold (TI_HANDLE hCmdBld, TI_UINT16 uFragmentThreshold, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).FragmentThreshold = uFragmentThreshold;  

    return cmdBld_CfgIeFragmentThreshold (hCmdBld, uFragmentThreshold, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgPreamble()
 ****************************************************************************
 * DESCRIPTION: Set the preamble in ?????? hardware register
 *
 * INPUTS:  
 *      preambleVal     
 * 
 * OUTPUT:  None
 * 
 * RETURNS: None
 ****************************************************************************/
TI_STATUS cmdBld_CfgPreamble (TI_HANDLE hCmdBld, Preamble_e ePreamble, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).preamble = ePreamble;      

    return cmdBld_CfgIePreamble (hCmdBld, (TI_UINT8)ePreamble, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgBcnBrcOptions()
 ****************************************************************************
 * DESCRIPTION: Configure the wlan hardware
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgBcnBrcOptions (TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeBcnBrcOptions (hCmdBld, pPMConfig, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgWakeUpCondition()
 ****************************************************************************
 * DESCRIPTION: Configure the wlan hardware
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgWakeUpCondition (TI_HANDLE hCmdBld, TPowerMgmtConfig *pPMConfig, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeWakeUpCondition (hCmdBld, pPMConfig, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgSleepAuth ()
 ****************************************************************************
 * DESCRIPTION: Set the min power level
 * 
 * INPUTS: 
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgSleepAuth (TI_HANDLE hCmdBld, EPowerPolicy eMinPowerLevel, void *fCb, TI_HANDLE hCb)
{
    /* Save th parameter in database */
    DB_WLAN(hCmdBld).minPowerLevel = eMinPowerLevel;

    return cmdBld_CfgIeSleepAuth (hCmdBld, eMinPowerLevel, fCb, hCb);
}


typedef enum { HW_CLOCK_40_MHZ = 40, HW_CLOCK_80_MHZ = 80 } EHwClock;


/****************************************************************************
 *                      cmdBld_CfgArmClock()
 ****************************************************************************
 * DESCRIPTION: Configure the arm clock
 *  !!! Note that the firmware will set the slot time according to the new clock
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgArmClock (TI_HANDLE hCmdBld, TI_UINT32 uArmClock, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);

    pWlanParams->ArmClock = uArmClock;

    /* Illegal combination Mac=80, Arm=40 ==> force setting 40/40*/
    if (pWlanParams->MacClock == HW_CLOCK_80_MHZ && pWlanParams->ArmClock == HW_CLOCK_40_MHZ)
    {
        TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_ArmClockSet: Illegal combination Mac=80, Arm=40 ==> force setting 40/40\n");
        pWlanParams->MacClock = HW_CLOCK_40_MHZ;
    }

    return cmdBld_CfgIeFeatureConfig (hCmdBld, 
                                      pWlanParams->FeatureOptions, 
                                      pWlanParams->FeatureDataFlowOptions,
                                      fCb, 
                                      hCb);
}


/****************************************************************************
 *                      cmdBld_CfgMacClock()
 ****************************************************************************
 * DESCRIPTION: Configure the mac clock
 *  !!! Note that the firmware will set the slot time according to the new clock
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgMacClock (TI_HANDLE hCmdBld, TI_UINT32 uMacClock, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);

    pWlanParams->MacClock = uMacClock;

    /* Force same clock - for printing */
    pWlanParams->ArmClock = uMacClock;

    /* Illegal combination Mac=80, Arm=40 ==> force setting 40/40*/
    if (pWlanParams->MacClock == HW_CLOCK_80_MHZ && pWlanParams->ArmClock == HW_CLOCK_40_MHZ)
    {
        TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_MacClockSet: Illegal combination Mac=80, Arm=40 ==> force setting 40/40\n");
        pWlanParams->MacClock = HW_CLOCK_40_MHZ;
    }

    return cmdBld_CfgIeFeatureConfig (hCmdBld, 
                                      pWlanParams->FeatureOptions, 
                                      pWlanParams->FeatureDataFlowOptions, 
                                      fCb, 
                                      hCb);
}


/****************************************************************************
 *                      cmdBld_CfgAid()
 ****************************************************************************
 * DESCRIPTION: Set the AID
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgAid (TI_HANDLE hCmdBld, TI_UINT16 uAidVal, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeAid (hCmdBld, uAidVal, fCb, hCb);
}

TI_STATUS cmdBld_CfgClkRun (TI_HANDLE hCmdBld, TI_BOOL aClkRunEnable, void *fCb, TI_HANDLE hCb)
{
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);

    if (aClkRunEnable)
    {
        pWlanParams->FeatureDataFlowOptions |= FEAT_PCI_CLK_RUN_ENABLE;
    }
    else
    {
        pWlanParams->FeatureDataFlowOptions &= ~FEAT_PCI_CLK_RUN_ENABLE;
    }

    return cmdBld_CfgIeFeatureConfig (hCmdBld, 
                                      pWlanParams->FeatureOptions, 
                                      pWlanParams->FeatureDataFlowOptions, 
                                      fCb, 
                                      hCb);
}


TI_STATUS cmdBld_CfgRxMsduFormat (TI_HANDLE hCmdBld, TI_BOOL bRxMsduForamtEnable, void *fCb, TI_HANDLE hCb)
{
#if 1
    /* WARNING:  Have to check how to control the Rx Frame format select (which bit)
                 and then access the HW*/
    return TI_OK;
#else
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);

    if (aRxMsduForamtEnable)
    {
        pWlanParams->FeatureDataFlowOptions |= DATA_FLOW_RX_MSDU_FRAME;
    }
    else
    {
        pWlanParams->FeatureDataFlowOptions &= ~DATA_FLOW_RX_MSDU_FRAME;
    }

    return cmdBld_CfgIeFeatureConfig (hCmdBld, 
                                      pWlanParams->FeatureOptions, 
                                      pWlanParams->FeatureDataFlowOptions, 
                                      fCb, 
                                      hCb);
#endif
}


/****************************************************************************
 *                      cmdBld_CfgTid()
 ****************************************************************************
 * DESCRIPTION: configure Queue traffic params
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgTid (TI_HANDLE hCmdBld, TQueueTrafficParams *pQtrafficParams, void *fCb, TI_HANDLE hCb)
{
    DB_QUEUES(hCmdBld).isQueueConfigured[pQtrafficParams->queueID] = TI_TRUE;
    DB_QUEUES(hCmdBld).queues[pQtrafficParams->queueID] = *pQtrafficParams;

    return cmdBld_CfgIeTid (hCmdBld, pQtrafficParams, fCb, hCb);  
}


/****************************************************************************
 *                      cmdBld_CfgAcParams()
 ****************************************************************************
 * DESCRIPTION: configure AC params
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgAcParams (TI_HANDLE hCmdBld, TAcQosParams *pAcQosParams, void *fCb, TI_HANDLE hCb)
{
    DB_AC(hCmdBld).isAcConfigured[pAcQosParams->ac] = TI_TRUE;
    DB_AC(hCmdBld).ac[pAcQosParams->ac] = *pAcQosParams;

    return cmdBld_CfgIeAcParams (hCmdBld, pAcQosParams, fCb, hCb);  
}


/****************************************************************************
 *                      cmdBld_CfgPsRxStreaming()
 ****************************************************************************
 * DESCRIPTION: configure PS-Rx-Streaming params
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgPsRxStreaming (TI_HANDLE hCmdBld, TPsRxStreaming *pPsRxStreaming, void *fCb, TI_HANDLE hCb)
{
    DB_PS_STREAM(hCmdBld).tid[pPsRxStreaming->uTid] = *pPsRxStreaming;

    return cmdBld_CfgIePsRxStreaming (hCmdBld, pPsRxStreaming, fCb, hCb);  
}


/****************************************************************************
 *                     cmdBld_CfgPacketDetectionThreshold
 ****************************************************************************
 * DESCRIPTION: Sets Packet Detection Threshold 
 * 
 * INPUTS:  None    
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgPacketDetectionThreshold (TI_HANDLE hCmdBld, TI_UINT32 threshold, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).PacketDetectionThreshold = threshold;

    return cmdBld_CfgIePacketDetectionThreshold (hCmdBld, threshold, fCb, hCb);
}


/****************************************************************************
 *                     cmdBld_CfgBeaconFilterOpt
 ****************************************************************************
 * DESCRIPTION: Sets Beacon filtering state 
 * 
 * INPUTS:  None    
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgBeaconFilterOpt (TI_HANDLE  hCmdBld, 
                                     TI_UINT8   uBeaconFilteringStatus, 
                                     TI_UINT8   uNumOfBeaconsToBuffer, 
                                     void       *fCb, 
                                     TI_HANDLE  hCb)
{
    DB_WLAN(hCmdBld).beaconFilterParams.desiredState = uBeaconFilteringStatus;
    DB_WLAN(hCmdBld).beaconFilterParams.numOfElements = uNumOfBeaconsToBuffer;

    return cmdBld_CfgIeBeaconFilterOpt (hCmdBld,
                                        uBeaconFilteringStatus,
                                        uNumOfBeaconsToBuffer,
                                        fCb,
                                        hCb);
}

/****************************************************************************
 *                     cmdBld_CfgRateMngDbg
 ****************************************************************************
 * DESCRIPTION: Sets rate managment params
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/

TI_STATUS cmdBld_CfgRateMngDbg (TI_HANDLE  hCmdBld,
                                RateMangeParams_t *pRateMngParams ,
                                void       *fCb,
                                TI_HANDLE  hCb)
{

	TRateMngParams      *pRateMngParamsDB = &DB_RM(hCmdBld);
	int uIndex;

	pRateMngParamsDB->rateMngParams.paramIndex = pRateMngParams->paramIndex;

	 switch (pRateMngParams->paramIndex)
		{
		case RATE_MGMT_RETRY_SCORE_PARAM:
			pRateMngParamsDB->rateMngParams.RateRetryScore = pRateMngParams->RateRetryScore;
			break;
		case RATE_MGMT_PER_ADD_PARAM:
			pRateMngParamsDB->rateMngParams.PerAdd = pRateMngParams->PerAdd;
			break;
		case RATE_MGMT_PER_TH1_PARAM:
			pRateMngParamsDB->rateMngParams.PerTh1 = pRateMngParams->PerTh1;
			break;
		case RATE_MGMT_PER_TH2_PARAM:
			pRateMngParamsDB->rateMngParams.PerTh2 = pRateMngParams->PerTh2;
			break;
		case RATE_MGMT_MAX_PER_PARAM:
			pRateMngParamsDB->rateMngParams.MaxPer = pRateMngParams->MaxPer;
			break;
		case RATE_MGMT_INVERSE_CURIOSITY_FACTOR_PARAM:
			pRateMngParamsDB->rateMngParams.InverseCuriosityFactor = pRateMngParams->InverseCuriosityFactor;
			break;
		case RATE_MGMT_TX_FAIL_LOW_TH_PARAM:
			pRateMngParamsDB->rateMngParams.TxFailLowTh = pRateMngParams->TxFailLowTh;
			break;
		case RATE_MGMT_TX_FAIL_HIGH_TH_PARAM:
			pRateMngParamsDB->rateMngParams.TxFailHighTh = pRateMngParams->TxFailHighTh;
			break;
		case RATE_MGMT_PER_ALPHA_SHIFT_PARAM:
			pRateMngParamsDB->rateMngParams.PerAlphaShift = pRateMngParams->PerAlphaShift;
			break;
		case RATE_MGMT_PER_ADD_SHIFT_PARAM:
			pRateMngParamsDB->rateMngParams.PerAddShift = pRateMngParams->PerAddShift;
			break;
		case RATE_MGMT_PER_BETA1_SHIFT_PARAM:
			pRateMngParamsDB->rateMngParams.PerBeta1Shift = pRateMngParams->PerBeta1Shift;
			break;
		case RATE_MGMT_PER_BETA2_SHIFT_PARAM:
			pRateMngParamsDB->rateMngParams.PerBeta2Shift = pRateMngParams->PerBeta2Shift;
			break;
		case RATE_MGMT_RATE_CHECK_UP_PARAM:
			pRateMngParamsDB->rateMngParams.RateCheckUp = pRateMngParams->RateCheckUp;
			break;
		case RATE_MGMT_RATE_CHECK_DOWN_PARAM:
			pRateMngParamsDB->rateMngParams.RateCheckDown = pRateMngParams->RateCheckDown;
			break;
	    case RATE_MGMT_RATE_RETRY_POLICY_PARAM:
			for (uIndex = 0; uIndex < 13; uIndex++)
				{
					pRateMngParamsDB->rateMngParams.RateRetryPolicy[uIndex] = pRateMngParams->RateRetryPolicy[uIndex];
				}
			break;
	 }


    return cmdBld_CfgIeRateMngDbg (hCmdBld,
                                   pRateMngParams,
                                   fCb,
                                   hCb);

}


/****************************************************************************
 *                     cmdBld_CfgBeaconFilterTable
 ****************************************************************************
 * DESCRIPTION: Sets Beacon filtering state 
 * 
 * INPUTS:  None    
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgBeaconFilterTable (TI_HANDLE    hCmdBld, 
                                       TI_UINT8     uNumberOfIEs, 
                                       TI_UINT8     *pIETable, 
                                       TI_UINT8     uIETableSize, 
                                       void         *fCb, 
                                       TI_HANDLE    hCb)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    if (uIETableSize > BEACON_FILTER_TABLE_MAX_SIZE)
    {
        TRACE2(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_CfgBeaconFilterTable: Table size is too big %d (>%d)\n", uIETableSize, BEACON_FILTER_TABLE_MAX_SIZE);

        return PARAM_VALUE_NOT_VALID;
    }

    os_memoryZero (pCmdBld->hOs, 
                   (void *)DB_WLAN(hCmdBld).beaconFilterIETable.IETable, 
                   BEACON_FILTER_TABLE_MAX_SIZE);
    os_memoryCopy (pCmdBld->hOs, 
                   (void *)DB_WLAN(hCmdBld).beaconFilterIETable.IETable, 
                   (void *)pIETable, 
                   uIETableSize);
    DB_WLAN(hCmdBld).beaconFilterIETable.numberOfIEs = uNumberOfIEs;
    DB_WLAN(hCmdBld).beaconFilterIETable.IETableSize = uIETableSize;

    return cmdBld_CfgIeBeaconFilterTable (hCmdBld, uNumberOfIEs, pIETable, uIETableSize, fCb, hCb);
}

 
/*----------------------------------------*/
/* Roaming Trigger Configuration Commands */
/*----------------------------------------*/

/****************************************************************************
 *                      cmdBld_CfgRssiSnrTrigger()
 ****************************************************************************
 * DESCRIPTION: Set the RSSI/SNR Trigger parameters.
 *
 * INPUTS:  
 *
 * OUTPUT:  None
 *
 * RETURNS: None
 ****************************************************************************/
TI_STATUS cmdBld_CfgRssiSnrTrigger (TI_HANDLE hCmdBld, RssiSnrTriggerCfg_t *pTriggerParam, void *fCb, TI_HANDLE hCb) 
{
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].index     = pTriggerParam->index;
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].threshold = pTriggerParam->threshold;
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].pacing    = pTriggerParam->pacing;
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].metric    = pTriggerParam->metric;
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].type      = pTriggerParam->type;
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].direction = pTriggerParam->direction;
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].hystersis = pTriggerParam->hystersis;
    DB_WLAN(hCmdBld).tRssiSnrTrigger[pTriggerParam->index].enable    = pTriggerParam->enable;

    return cmdBld_CfgIeRssiSnrTrigger (hCmdBld, pTriggerParam, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgRssiSnrWeights()
 ****************************************************************************
 * DESCRIPTION: Set RSSI/SNR Weights for Average calculations.
 *
 * INPUTS:  
 *
 * OUTPUT:  None
 *
 * RETURNS: None
 ****************************************************************************/
TI_STATUS cmdBld_CfgRssiSnrWeights (TI_HANDLE hCmdBld, RssiSnrAverageWeights_t *pWeightsParam, void *fCb, TI_HANDLE hCb) 
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "\n cmdBld_CfgRssiSnrWeights :\n                             uRssiBeaconAverageWeight = %d\n                             uRssiPacketAverageWeight = %d\n                             uSnrBeaconAverageWeight  = %d\n                             uSnrPacketAverageWeight = %d \n ", pWeightsParam->rssiBeaconAverageWeight, pWeightsParam->rssiPacketAverageWeight, pWeightsParam->snrBeaconAverageWeight , pWeightsParam->snrPacketAverageWeight  );

    DB_WLAN(hCmdBld).tRssiSnrWeights.rssiBeaconAverageWeight = pWeightsParam->rssiBeaconAverageWeight;
    DB_WLAN(hCmdBld).tRssiSnrWeights.rssiPacketAverageWeight = pWeightsParam->rssiPacketAverageWeight;
    DB_WLAN(hCmdBld).tRssiSnrWeights.snrBeaconAverageWeight  = pWeightsParam->snrBeaconAverageWeight ;
    DB_WLAN(hCmdBld).tRssiSnrWeights.snrPacketAverageWeight  = pWeightsParam->snrPacketAverageWeight ;

    return cmdBld_CfgIeRssiSnrWeights (hCmdBld, pWeightsParam, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgMaxTxRetry()
 ****************************************************************************
 * DESCRIPTION: Set Max Tx retry parmaters.
 *
 * INPUTS:  
 *      maxTxRetry             max Tx Retry
 *
 * OUTPUT:  None
 *
 * RETURNS: None
 ****************************************************************************/
TI_STATUS cmdBld_CfgMaxTxRetry (TI_HANDLE hCmdBld, TRroamingTriggerParams *pRoamingTriggerCmd, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).roamTriggers.maxTxRetry = pRoamingTriggerCmd->maxTxRetry;

    return cmdBld_CfgIeMaxTxRetry (hCmdBld, pRoamingTriggerCmd, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgSgEnable()
 ****************************************************************************
 * DESCRIPTION: Save Soft Gemini enable parameter
 *
 * INPUTS:
 *
 * OUTPUT:
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgSgEnable (TI_HANDLE hCmdBld, ESoftGeminiEnableModes eSgEnable, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).SoftGeminiEnable = eSgEnable;
                    
    return cmdBld_CfgIeSgEnable (hCmdBld, eSgEnable, fCb, hCb);
}


/****************************************************************************
 *                      cmdBld_CfgSg()
 ****************************************************************************
 * DESCRIPTION: Save Soft Gemini config parameter
 *
 * INPUTS:
 *
 * OUTPUT:
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgSg (TI_HANDLE hCmdBld, TSoftGeminiParams *pSgParam, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    /* Copy params for recovery */
    os_memoryCopy (pCmdBld->hOs,
                   (void*)&DB_WLAN(hCmdBld).SoftGeminiParams,
                   (void*)pSgParam,
                   sizeof(TSoftGeminiParams));
                  
    return cmdBld_CfgIeSg (hCmdBld, pSgParam, fCb, hCb);
}

/****************************************************************************
 *                      cmdBld_CfgCoexActivity()
 ****************************************************************************
 * DESCRIPTION: Sets the CoexActivity table. 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK  pWlanParams->RtsThreshold
 ****************************************************************************/
TI_STATUS cmdBld_CfgCoexActivity (TI_HANDLE hCmdBld, TCoexActivity *pCoexActivity, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);
    TCoexActivity *pSaveCoex = &pWlanParams->tWlanParamsCoexActivityTable.entry[0];
    int numOfElements = pWlanParams->tWlanParamsCoexActivityTable.numOfElements;
    int i;
    
    /* Check if to overwrite existing entry or put on last index */
    for (i=0; i<numOfElements; i++)
    {
        if ((pSaveCoex[i].activityId == pCoexActivity->activityId) && (pSaveCoex->coexIp == pCoexActivity->coexIp))
        {
            break;
        }
    }

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "cmdBld_CfgCoexActivity: save Param %d in index %d, %d %d\n", 0, i, pCoexActivity->coexIp, pCoexActivity->activityId);
    /* save in WlanParams for recovery */
    pSaveCoex[i].coexIp          = pCoexActivity->coexIp;
    pSaveCoex[i].activityId      = pCoexActivity->activityId;
    pSaveCoex[i].defaultPriority = pCoexActivity->defaultPriority;
    pSaveCoex[i].raisedPriority  = pCoexActivity->raisedPriority;
    pSaveCoex[i].minService      = pCoexActivity->minService;
    pSaveCoex[i].maxService      = pCoexActivity->maxService;

    if (i == numOfElements)
    {
        /* no existing entry overwrite, increment number of elements */
        pWlanParams->tWlanParamsCoexActivityTable.numOfElements++;
    }

    return cmdBld_CfgIeCoexActivity (hCmdBld, pCoexActivity, fCb, hCb);
}

/****************************************************************************
 *                      cmdBld_CfgFmCoex()
 ****************************************************************************
 * DESCRIPTION: Save and configure FM coexistence parameters
 *
 * INPUTS:
 *
 * OUTPUT:
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgFmCoex (TI_HANDLE hCmdBld, TFmCoexParams *pFmCoexParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    /* Copy params for recovery */
    os_memoryCopy (pCmdBld->hOs,
                   (void*)&(DB_WLAN(hCmdBld).tFmCoexParams),
                   (void*)pFmCoexParams,
                   sizeof(TFmCoexParams));

    return cmdBld_CfgIeFmCoex (hCmdBld, pFmCoexParams, fCb, hCb);
}

/****************************************************************************
 *                      cmdBld_CfgTxRatePolicy()
 ****************************************************************************
 * DESCRIPTION: configure TxRatePolicy params
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgTxRatePolicy (TI_HANDLE hCmdBld, TTxRatePolicy *pTxRatePolicy, void *fCb, TI_HANDLE hCb)
{
    TCmdBld       *pCmdBld      = (TCmdBld *)hCmdBld;
    TTxRateClass  *pTxRateClass = pTxRatePolicy->rateClass;
    TI_UINT8       index;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "_1, Num of classes = 0x%x\n", pTxRatePolicy->numOfRateClasses);

    DB_BSS(hCmdBld).TxRateClassParams.numOfRateClasses = pTxRatePolicy->numOfRateClasses;

    for (index = 0; index < pTxRatePolicy->numOfRateClasses; index++, pTxRateClass++)
    {
        TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "_2loop, Index = %d, Short R = 0x%x, Long R = 0x%x, Rates = 0x%x\n", index, pTxRateClass->shortRetryLimit, pTxRateClass->longRetryLimit, pTxRateClass->txEnabledRates);

        DB_BSS(hCmdBld).TxRateClassParams.rateClass[index] = *pTxRateClass;
    }

    return cmdBld_CfgIeTxRatePolicy (hCmdBld, pTxRatePolicy, fCb, hCb);  
}


TI_STATUS cmdBld_CfgSlotTime (TI_HANDLE hCmdBld, ESlotTime eSlotTime, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    DB_WLAN(hCmdBld).SlotTime = eSlotTime;      

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CfgSlotTime: Slot time = %d\n", eSlotTime);

    /* Configure the new Slot-Time value to the FW. */
    return cmdBld_CfgIeSlotTime (hCmdBld, (TI_UINT8)eSlotTime, fCb, hCb);
}


TI_STATUS cmdBld_CfgEventMask (TI_HANDLE hCmdBld, TI_UINT32 uEventMask, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeEventMask (hCmdBld, uEventMask, fCb, hCb);
}


/*
 * ----------------------------------------------------------------------------
 * Function : cmdBld_CfgHwEncEnable
 *
 * Input    : 
 * Output   :
 * Process  :
 * Note(s)  :
 * -----------------------------------------------------------------------------
 */
TI_STATUS cmdBld_CfgHwEncEnable (TI_HANDLE hCmdBld, TI_BOOL bHwEncEnable, TI_BOOL bHwDecEnable, void *fCb, TI_HANDLE hCb)
{
    TCmdBld     *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams *pWlanParams = &DB_WLAN(hCmdBld);
    
    /* Store the HW encryption Enable flag for reconfigure time (FW reload)*/
    DB_KEYS(pCmdBld).bReconfHwEncEnable = bHwEncEnable;
    DB_KEYS(pCmdBld).bHwEncDecrEnableValid = TI_TRUE;

    if (bHwEncEnable)
    {
        pWlanParams->FeatureDataFlowOptions &= ~DF_ENCRYPTION_DISABLE;
    }
    else
    {
        pWlanParams->FeatureDataFlowOptions |= DF_ENCRYPTION_DISABLE;
    }

    /* Set bit DF_SNIFF_MODE_ENABLE to enable or prevent decryption in fw */
    /* WARNING: Have to check how to control the decryption (which bit) and then set/reset
                the  appropriate bit*/ 
    if (bHwDecEnable)
    {
        pWlanParams->FeatureDataFlowOptions &= ~DF_SNIFF_MODE_ENABLE;
    }
    else
    {
        pWlanParams->FeatureDataFlowOptions |= DF_SNIFF_MODE_ENABLE;
    }

    return cmdBld_CfgIeFeatureConfig (hCmdBld, 
                                      pWlanParams->FeatureOptions, 
                                      pWlanParams->FeatureDataFlowOptions, 
                                      fCb, 
                                      hCb);
}


TI_STATUS cmdBld_CfgHwEncDecEnable (TI_HANDLE hCmdBld, TI_BOOL bHwEncEnable, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgHwEncEnable (hCmdBld, bHwEncEnable, bHwEncEnable, fCb, hCb);
}


/*
 * ----------------------------------------------------------------------------
 * Function : cmdBld_CfgSecureMode
 *
 * Input    : 
 * Output   :
 * Process  :
 * Note(s)  :
 * -----------------------------------------------------------------------------
 */
TI_STATUS cmdBld_CfgSecureMode (TI_HANDLE hCmdBld, ECipherSuite eSecurMode, void *fCb, TI_HANDLE hCb)
{
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32     index;

    if (eSecurMode < TWD_CIPHER_MAX)
    {
        TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "cmdBld_CfgSecureMode: change tSecurity mode from %d --> %d\n", pCmdBld->tSecurity.eSecurityMode, eSecurMode);
        /* check if tSecurity mode is equal to previous one*/
        if (pCmdBld->tSecurity.eSecurityMode == eSecurMode)
        {
            return TI_OK;
        }

        /* Reset all reconfig valid fields*/
        DB_KEYS(pCmdBld).bHwEncDecrEnableValid = TI_FALSE;
        DB_KEYS(pCmdBld).bDefaultKeyIdValid = TI_FALSE;  
        for (index = 0; 
             index < pCmdBld->tSecurity.uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION + NO_OF_EXTRA_RECONF_SECUR_KEYS; 
             index++)
        {
            (DB_KEYS(pCmdBld).pReconfKeys + index)->keyType = KEY_NULL;
        }
        
        /* set the new tSecurity mode*/
        pCmdBld->tSecurity.eSecurityMode = eSecurMode;

        /* disable defrag, duplicate detection on TNETW+XCC on chip level*/
        /* YV- to add fragmentation control (if there is- artur ?)*/
        return cmdBld_CfgRxMsduFormat (hCmdBld, 
                                       pCmdBld->tSecurity.eSecurityMode != TWD_CIPHER_CKIP,
                                       fCb,
                                       hCb);
    }
    else
    {
        return TI_NOK;
    }
}


TI_STATUS cmdBld_CfgConnMonitParams (TI_HANDLE  hCmdBld, 
                                     TRroamingTriggerParams *pRoamingTriggerCmd, 
                                     void       *fCb, 
                                     TI_HANDLE  hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE2(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "SetBssLossTsfThresholdParamsCmd :\n                             BssLossTimeout = %d\n                             TsfMissThreshold = %d \n ", pRoamingTriggerCmd->BssLossTimeout, pRoamingTriggerCmd->TsfMissThreshold);

    DB_WLAN(hCmdBld).roamTriggers.BssLossTimeout = pRoamingTriggerCmd->BssLossTimeout;
    DB_WLAN(hCmdBld).roamTriggers.TsfMissThreshold = pRoamingTriggerCmd->TsfMissThreshold;

    return cmdBld_CfgIeConnMonitParams (hCmdBld, pRoamingTriggerCmd, fCb, hCb);
}


/****************************************************************************
 *                 cmdBld_CfgEnableRxDataFilter()
 ****************************************************************************
 * DESCRIPTION: Enables or disables Rx data filtering.
 * 
 * INPUTS:  enabled             - 0 to disable data filtering, any other value to enable.
 *          defaultAction       - The default action to take on non-matching packets.
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgEnableRxDataFilter (TI_HANDLE   hCmdBld, 
                                        TI_BOOL     bEnabled, 
                                        filter_e    eDefaultAction, 
                                        void        *fCb, 
                                        TI_HANDLE   hCb)
{
    /* Save parameters for reconfig phase */
    DB_RX_DATA_FLTR(hCmdBld).bEnabled       = bEnabled;
    DB_RX_DATA_FLTR(hCmdBld).eDefaultAction = eDefaultAction;

    return cmdBld_CfgIeEnableRxDataFilter (hCmdBld, bEnabled, eDefaultAction, fCb, hCb);
}


/****************************************************************************
*                      cmdBld_CfgRxDataFilter()
*****************************************************************************
* DESCRIPTION: Add/remove Rx Data filter information element.
*
* INPUTS:  index               - Index of the Rx Data filter
*          command             - Add or remove the filter
*          action              - Action to take on packets matching the pattern
*          numFieldPatterns    - Number of field patterns in the filter
*          lenFieldPatterns    - Length of the field pattern series
*          fieldPatterns       - Series of field patterns
*
* OUTPUT:  None
*
* RETURNS: TI_OK or TI_NOK
****************************************************************************/
TI_STATUS cmdBld_CfgRxDataFilter (TI_HANDLE hCmdBld, 
                                  TI_UINT8     index, 
                                  TI_UINT8     command, 
                                  filter_e  eAction, 
                                  TI_UINT8     uNumFieldPatterns, 
                                  TI_UINT8     uLenFieldPatterns, 
                                  TI_UINT8  *pFieldPatterns, 
                                  void      *fCb, 
                                  TI_HANDLE hCb)
{
    TCmdBld         *pCmdBld  = (TCmdBld *)hCmdBld;
    TRxDataFilter   *pFilters = &(DB_RX_DATA_FLTR(hCmdBld).aRxDataFilter[index]);

    /* Save parameters for reconfig phase */
    pFilters->uIndex            = index;
    pFilters->uCommand          = command;
    pFilters->eAction           = eAction;
    pFilters->uNumFieldPatterns = uNumFieldPatterns;
    pFilters->uLenFieldPatterns = uLenFieldPatterns;
    os_memoryCopy(pCmdBld->hOs, pFilters->aFieldPattern, pFieldPatterns, uLenFieldPatterns);

    return cmdBld_CfgIeRxDataFilter (hCmdBld, 
                                     index, 
                                     command, 
                                     eAction, 
                                     uNumFieldPatterns, 
                                     uLenFieldPatterns, 
                                     pFieldPatterns, 
                                     fCb, 
                                     hCb);
}


TI_STATUS cmdBld_CfgCtsProtection (TI_HANDLE hCmdBld, TI_UINT8 uCtsProtection, void *fCb, TI_HANDLE hCb)
    {
        DB_WLAN(hCmdBld).CtsToSelf = uCtsProtection;

    return cmdBld_CfgIeCtsProtection (hCmdBld, uCtsProtection, fCb, hCb);
}


TI_STATUS cmdBld_CfgServicePeriodTimeout (TI_HANDLE hCmdBld, TRxTimeOut *pRxTimeOut, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeServicePeriodTimeout (hCmdBld, pRxTimeOut, fCb, hCb);
}


TI_STATUS cmdBld_CfgRxMsduLifeTime (TI_HANDLE hCmdBld, TI_UINT32 uRxMsduLifeTime, void *fCb, TI_HANDLE hCb)
    {
        DB_WLAN(hCmdBld).MaxRxMsduLifetime = uRxMsduLifeTime;

    return cmdBld_CfgIeRxMsduLifeTime (hCmdBld, uRxMsduLifeTime, fCb, hCb);
}


TI_STATUS cmdBld_CfgStatisitics (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeStatisitics (hCmdBld, fCb, hCb);
}


TI_STATUS cmdBld_CfgTxPowerDbm (TI_HANDLE hCmdBld, TI_UINT8 uTxPowerDbm, void *fCb, TI_HANDLE hCb)
{
    return cmdBld_CfgIeTxPowerDbm (hCmdBld, uTxPowerDbm, fCb, hCb);
}

 /*
 * ----------------------------------------------------------------------------
 * Function : cmdBld_CfgBet
 *
 * Input    :   enabled               - 0 to disable BET, 0 to disable BET
 *              MaximumConsecutiveET  - Max number of consecutive beacons
 *                                      that may be early terminated.
 * Output   : TI_STATUS
 * Process  :  Configures Beacon Early Termination information element.
 * Note(s)  :  None
 * -----------------------------------------------------------------------------
 */
TI_STATUS cmdBld_CfgBet (TI_HANDLE hCmdBld, TI_UINT8 Enable, TI_UINT8 MaximumConsecutiveET, void *fCb, TI_HANDLE hCb)
{
    DB_WLAN(hCmdBld).BetEnable              = Enable;
    DB_WLAN(hCmdBld).MaximumConsecutiveET   = MaximumConsecutiveET;

    return cmdBld_CfgIeBet (hCmdBld, Enable, MaximumConsecutiveET, fCb, hCb);
}

/****************************************************************************
 *                      cmdBld_CfgKeepAlive()
 ****************************************************************************
 * DESCRIPTION: Set keep-alive paramters for a single index
 * 
 * INPUTS: Paramters and CB
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgKeepAlive (TI_HANDLE hCmdBld, TKeepAliveParams *pKeepAliveParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE4(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "CmdBld: Seeting keep-alive params, index=%d, interval=%d msec, trigType=%d, enaFlag=%d\n", pKeepAliveParams->index, pKeepAliveParams->interval, pKeepAliveParams->trigType, pKeepAliveParams->enaDisFlag);

    os_memoryCopy (pCmdBld->hOs, 
                   (void *)&DB_KLV(hCmdBld).keepAliveParams[ pKeepAliveParams->index ], 
                   (void *)pKeepAliveParams, 
                   sizeof (TKeepAliveParams));

    return cmdBld_CmdIeConfigureKeepAliveParams (hCmdBld, 
                                                 pKeepAliveParams->index,
                                                 pKeepAliveParams->enaDisFlag,
                                                 (TI_UINT8)pKeepAliveParams->trigType,
                                                 pKeepAliveParams->interval,
                                                 fCb,
                                                 hCb);
}

/****************************************************************************
 *                      cmdBld_CfgKeepAliveEnaDis()
 ****************************************************************************
 * DESCRIPTION: Set global keep-alive enable / disable flag
 * 
 * INPUTS: Paramters and CB
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CfgKeepAliveEnaDis(TI_HANDLE hCmdBld, TI_UINT8 enaDisFlag, void *fCb, TI_HANDLE hCb)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "CmdBld: Seeting keep-alive Global ena / dis flag to %d\n", (TI_UINT32)enaDisFlag);

    DB_KLV(hCmdBld).enaDisFlag = enaDisFlag;

    return cmdBld_CmdIeConfigureKeepAliveEnaDis (hCmdBld, enaDisFlag, fCb, hCb);
}

/** 
 * \fn     cmdBld_CfgSetFwHtCapabilities 
 * \brief  set the current AP HT Capabilities to the FW. 
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgSetFwHtCapabilities (TI_HANDLE hCmdBld,
                                         TI_UINT32 uHtCapabilites,
                                         TMacAddr  tMacAddress,
                                         TI_UINT8  uAmpduMaxLeng,
                                         TI_UINT8  uAmpduMinSpac,
                                         void      *fCb, 
                                         TI_HANDLE hCb)
{

    DB_BSS(hCmdBld).bHtCap = TI_TRUE;
    DB_BSS(hCmdBld).uHtCapabilites = uHtCapabilites;
    MAC_COPY ((DB_BSS(hCmdBld).tMacAddress), tMacAddress);
    DB_BSS(hCmdBld).uAmpduMaxLeng = uAmpduMaxLeng;
    DB_BSS(hCmdBld).uAmpduMinSpac = uAmpduMinSpac;

    return cmdBld_CfgIeSetFwHtCapabilities (hCmdBld,
                                            uHtCapabilites,
                                            tMacAddress,
                                            uAmpduMaxLeng,
                                            uAmpduMinSpac,
                                            fCb, 
                                            hCb);
}

/** 
 * \fn     cmdBld_CfgSetFwHtInformation 
 * \brief  set the current AP HT Information to the FW. 
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgSetFwHtInformation (TI_HANDLE hCmdBld,
                                        TI_UINT8  uRifsMode,         
                                        TI_UINT8  uHtProtection,     
                                        TI_UINT8  uGfProtection,     
                                        TI_UINT8  uHtTxBurstLimit,   
                                        TI_UINT8  uDualCtsProtection,
                                        void      *fCb, 
                                        TI_HANDLE hCb)
{

    DB_BSS(hCmdBld).bHtInf = TI_TRUE;
    DB_BSS(hCmdBld).uRifsMode = uRifsMode;
    DB_BSS(hCmdBld).uHtProtection = uHtProtection;
    DB_BSS(hCmdBld).uGfProtection = uGfProtection;
    DB_BSS(hCmdBld).uHtTxBurstLimit = uHtTxBurstLimit;
    DB_BSS(hCmdBld).uDualCtsProtection = uDualCtsProtection;

    return cmdBld_CfgIeSetFwHtInformation (hCmdBld,
                                           uRifsMode,          
                                           uHtProtection,      
                                           uGfProtection,      
                                           uHtTxBurstLimit,    
                                           uDualCtsProtection, 
                                           fCb, 
                                           hCb);
}

/** 
 * \fn     cmdBld_CfgSetBaInitiator 
 * \brief  configure BA session initiator\receiver parameters setting in the FW. 
 *
 * \note    
 * \return TI_OK on success or TI_NOK on failure 
 * \sa 
 */ 
TI_STATUS cmdBld_CfgSetBaSession (TI_HANDLE hCmdBld, 
                                  InfoElement_e eBaType,   
                                  TI_UINT8 uTid,               
                                  TI_UINT8 uState,             
                                  TMacAddr tRa,                
                                  TI_UINT16 uWinSize,          
                                  TI_UINT16 uInactivityTimeout,
                                  void *fCb, 
                                  TI_HANDLE hCb)
{
    if (ACX_BA_SESSION_INITIATOR_POLICY == eBaType)
    {
        DB_BSS(hCmdBld).bBaInitiator[uTid] = TI_TRUE;
        DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].uTid = uTid;
        DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].uPolicy = uState;
        MAC_COPY ((DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].aMacAddress),tRa);
        DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].uWinSize = uWinSize;
        DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].uInactivityTimeout = uInactivityTimeout;
    }
    else
    {
        DB_BSS(hCmdBld).bBaResponder[uTid] = TI_TRUE;
        DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].uTid = uTid;
        DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].uPolicy = uState;
        MAC_COPY ((DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].aMacAddress),tRa);
        DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].uWinSize = uWinSize;
        DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].uInactivityTimeout = uInactivityTimeout;
    }

    return cmdBld_CfgIeSetBaSession (hCmdBld, 
                                     eBaType,
                                     uTid,               
                                     uState,             
                                     tRa,                
                                     uWinSize,          
                                     uInactivityTimeout,
                                     fCb,
                                     hCb);
}


TI_STATUS cmdBld_CfgBurstMode (TI_HANDLE hCmdBld, TI_BOOL bEnabled, void *fCb, TI_HANDLE hCb)
{
	DB_AC(hCmdBld).isBurstModeEnabled = bEnabled;
	return cmdBld_CfgIeBurstMode (hCmdBld, bEnabled, fCb, hCb); 
}

