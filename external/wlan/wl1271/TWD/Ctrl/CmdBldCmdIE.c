/*
 * CmdBldCmdIE.c
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


/** \file  CmdBldCmdIE.c 
 *  \brief Command builder. Command information elements
 *
 *  \see   CmdBldCmdIE.h 
 */
#define __FILE_ID__  FILE_ID_94
#include "osApi.h"
#include "tidef.h"
#include "report.h"
#include "TWDriver.h"
#include "CmdQueue_api.h"
#include "CmdBld.h"


/* Local Macros */

#define MAC_TO_VENDOR_PREAMBLE(mac) ((mac[0] << 16) | (mac[1] << 8) | mac[2])

/*******************************************
 * Wlan hardware Test (BIT)
 * =================
 *
 * Tests description:
 * ==================
 * FCC           = Continuous modulated transmission (should not emit carrier)
 * TELEC         = Continuous unmodulated carrier transmission (carrier only)
 * PER_TX_STOP   = Stops the TX test in progress (FCC or TELEC).
 * ReadRegister  = Read a register value.
 * WriteRegister = Sets a register value.
* 
* Rx PER test
* ========
* PerRxStart       = Start or resume the PER measurement. This function will put the device in promiscuous mode, and resume counters update.
* PerRxStop        = Stop Rx PER measurements. This function stop counters update and make it is safe to read the PER test result.
* PerRxGetResults  = Get the last Rx PER test results.
* PerRxClear       = Clear the Rx PER test results. 
 */

enum
{
/* 0 */   TEST_MOD_QPSK,
/* 1 */   TEST_MOD_CCK,
/* 2 */   TEST_MOD_PBCC,
          TEST_MOD_NUMOF
};

enum
{
/* 0 */   TEST_MOD_LONG_PREAMBLE,
/* 1 */   TEST_MOD_SHORT_PREAMBLE
};

enum
{
/* 0 */   TEST_BAND_2_4GHZ,
/* 1 */   TEST_BAND_5GHZ,
/* 2 */   TEST_BAND_4_9GHZ
};


enum
{
    MOD_PBCC = 1,
    MOD_CCK,
    MOD_OFDM
};


#define TEST_MOD_MIN_GAP           200
#define TEST_MOD_MIN_TX_BODYLEN    0
#define TEST_MOD_MAX_TX_BODYLEN    2304

#define TEST_RX_CAL_SAFE_TIME      5000  /*uSec*/

#define TEST_MOD_IS_GAP_OK(gap)     ((gap) >= TEST_MOD_MIN_GAP)

#define TEST_MOD_IS_TX_BODYLEN_OK(len)  \
   (INRANGE((len), TEST_MOD_MIN_TX_BODYLEN, TEST_MOD_MAX_TX_BODYLEN) && \
   (((len) & 3) == 0) )

#define TEST_MOD_IS_PREAMBLE_OK(p)  \
   INRANGE((p), TEST_MOD_LONG_PREAMBLE, TEST_MOD_SHORT_PREAMBLE)


#define RESEARVED_SIZE_FOR_RESPONSE 4


/****************************************************************************
 *                      cmdBld_CmdIeStartBss()
 ****************************************************************************
 * DESCRIPTION: Construct the StartBss command fileds and send it to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeStartBss (TI_HANDLE hCmdBld, BSS_e BssType, void *fJoinCompleteCb, TI_HANDLE hCb)
{
    TCmdBld            *pCmdBld = (TCmdBld *)hCmdBld;
    StartJoinRequest_t  AcxCmd_StartBss;
    StartJoinRequest_t *pCmd = &AcxCmd_StartBss;
    TSsid              *pSsid = &DB_BSS(hCmdBld).tSsid;
    TBssInfoParams     *pBssInfoParams = &DB_BSS(hCmdBld);
    TI_UINT8 *BssId;
    TI_UINT8 *cmdBssId;
    EHwRateBitFiled HwBasicRatesBitmap;
    TI_UINT32 i; 
    
    os_memoryZero (pCmdBld->hOs, (void *)pCmd, sizeof(StartJoinRequest_t));

    /*
     * Set RxCfg and RxFilterCfg values
     */
    pCmd->rxFilter.ConfigOptions = ENDIAN_HANDLE_LONG (DB_WLAN(hCmdBld).RxConfigOption);
    pCmd->rxFilter.FilterOptions = ENDIAN_HANDLE_LONG (DB_WLAN(hCmdBld).RxFilterOption);
    pCmd->beaconInterval = ENDIAN_HANDLE_WORD (DB_BSS(hCmdBld).BeaconInterval);
    pCmd->dtimInterval = DB_BSS(hCmdBld).DtimInterval; 
    pCmd->channelNumber = DB_BSS(hCmdBld).RadioChannel;
    pCmd->bssType = BssType;
    /* Add radio band */
    pCmd->bssType |= DB_WLAN(hCmdBld).RadioBand << 4;
    /* Bits 0-2: Tx-Session-Count. bit 7: indicates if to flush the Tx queues */
    pCmd->ctrl = pBssInfoParams->Ctrl; 
    
    /*
     * BasicRateSet
     * The wlan hardware uses pHwMboxCmd field to determine the rate at which to transmit 
     * control frame responses (such as ACK or CTS frames)
     */
    cmdBld_ConvertAppRatesBitmap (pBssInfoParams->BasicRateSet, 0, &HwBasicRatesBitmap);
    pCmd->basicRateSet = ENDIAN_HANDLE_LONG(HwBasicRatesBitmap);

    /* BSS ID - reversed order (see wlan hardware spec) */
    BssId = DB_BSS(hCmdBld).BssId;
    cmdBssId = (TI_UINT8*)&pCmd->bssIdL;
    for (i = 0; i < MAC_ADDR_LEN; i++)
        cmdBssId[i] = BssId[MAC_ADDR_LEN - 1 - i];

    /* SSID string */ 
    pCmd->ssidLength = pSsid->len;
    os_memoryCopy (pCmdBld->hOs, (void *)pCmd->ssidStr, (void *)pSsid->str, pSsid->len);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_START_JOIN, 
                             (TI_CHAR *)pCmd, 
                             sizeof(*pCmd), 
                             fJoinCompleteCb, 
                             hCb, 
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeEnableRx()
 ****************************************************************************
 * DESCRIPTION: Construct the EnableRx command fileds and send it to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeEnableRx (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld  *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8  aEnableRx_buf[4];

    aEnableRx_buf[0] = DB_DEFAULT_CHANNEL (hCmdBld);
    aEnableRx_buf[1] = 0; /* padding */
    aEnableRx_buf[2] = 0; /* padding */
    aEnableRx_buf[3] = 0; /* padding */


    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_ENABLE_RX, 
                             (TI_CHAR *)aEnableRx_buf, 
                             sizeof(aEnableRx_buf),
                             fCb,
                             hCb,
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeEnableTx()
 ****************************************************************************
 * DESCRIPTION: Construct the EnableTx command fileds and send it to the mailbox
 *              Note: This Enable_TX command is used also for changing the serving 
 *              channel.
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeEnableTx (TI_HANDLE hCmdBld, TI_UINT8 channel, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8  aEnableTx_buf[4];

    aEnableTx_buf[0] = channel;
    aEnableTx_buf[1] = 0; /* padding */
    aEnableTx_buf[2] = 0; /* padding */
    aEnableTx_buf[3] = 0; /* padding */

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_ENABLE_TX, 
                             (TI_CHAR *)aEnableTx_buf, 
                             sizeof(aEnableTx_buf),
                             fCb,
                             hCb,
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeDisableRx()
 ****************************************************************************
 * DESCRIPTION: Construct the DisableRx command fileds and send it to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeDisableRx (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_DISABLE_RX, NULL, 0, fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CmdIeDisableTx()
 ****************************************************************************
 * DESCRIPTION: Construct the DisableTx command fileds and send it to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeDisableTx (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_DISABLE_TX, NULL, 0, fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CmdIeConfigureTemplateFrame()
 ****************************************************************************
 * DESCRIPTION: Generic function which sets the Fw with a template frame according
 *              to the given template type.
 * 
 * INPUTS: templateType - CMD_BEACON, CMD_PROBE_REQ, CMD_PROBE_RESP etc.
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeConfigureTemplateFrame (TI_HANDLE         hCmdBld, 
                                              TTemplateParams  *pTemplate, 
                                              TI_UINT16         uFrameSize, 
                                              TemplateType_e    eTemplateType, 
                                              TI_UINT8          uIndex, 
                                              void *            fCb, 
                                              TI_HANDLE         hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    PktTemplate_t AcxCmd_PktTemplate;
    PktTemplate_t *pCmd = &AcxCmd_PktTemplate;

    /* If the frame size is too big - we truncate the frame template */
    if (uFrameSize > MAX_TEMPLATES_SIZE)
    {
        EReportSeverity eReportSeverity = (pTemplate == NULL) ? REPORT_SEVERITY_WARNING : REPORT_SEVERITY_ERROR;

        /* Report as error only if this is the actual template and not just a space holder */
        TRACE3(pCmdBld->hReport, eReportSeverity, "cmdBld_CmdIeConfigureTemplateFrame: Frame size (=%d) of CmdType (=%d) is bigger than MAX_TEMPLATES_SIZE(=%d) !!!\n", uFrameSize, eTemplateType, MAX_TEMPLATES_SIZE);

        /* Truncate length to the template size limit */
        uFrameSize = MAX_TEMPLATES_SIZE;
    }
    /* WLAN_OS_REPORT(("DloadTempl type =%d size=%d\n", eTemplateType, uFrameSize)); */
    /* if pTemplate is NULL than it means that we just want to reserve place in Fw, and there is no need to copy */
    if (pTemplate != NULL)
    {
        os_memoryCopy(pCmdBld->hOs, (void *)&pCmd->templateStart, (void *)(pTemplate->Buffer), uFrameSize);
        pCmd->templateTxAttribute.enabledRates    = pTemplate->uRateMask;
    }
    pCmd->len = ENDIAN_HANDLE_WORD(uFrameSize);
    pCmd->index = uIndex;
    pCmd->templateType = eTemplateType;
    pCmd->templateTxAttribute.shortRetryLimit = 10;
    pCmd->templateTxAttribute.longRetryLimit  = 10;

#ifdef TI_DBG
    if (pCmdBld->uDbgTemplatesRateMask != 0) 
    {
        pCmd->templateTxAttribute.enabledRates = pCmdBld->uDbgTemplatesRateMask;
    }
#endif

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_SET_TEMPLATE, 
                             (TI_CHAR *)pCmd, 
                             sizeof (PktTemplate_t),
                             fCb,
                             hCb,
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeSetKey()
 ****************************************************************************
 * DESCRIPTION: Construct the SetKey command fileds and send it to the mailbox
 * 
 * INPUTS: 
 *      Action      - add/remove key
 *      MacAddr     - relevant only for mapping keys
 *      KeySize     - key size
 *      KeyType     - default/mapping/TKIP
 *      KeyId       - relevant only for default keys
 *      Key         - key data
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeSetKey (TI_HANDLE hCmdBld, 
                              TI_UINT32 action, 
                              TI_UINT8  *pMacAddr, 
                              TI_UINT32 uKeySize, 
                              TI_UINT32 uKeyType, 
                              TI_UINT32 uKeyId, 
                              TI_UINT8  *pKey, 
                              TI_UINT32 uSecuritySeqNumLow, 
                              TI_UINT32 uSecuritySeqNumHigh,
                              void      *fCb, 
                              TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    SetKey_t AcxCmd_SetKey;
    SetKey_t *pCmd = &AcxCmd_SetKey;

    os_memoryZero (pCmdBld->hOs, (void *)pCmd, sizeof(*pCmd));

    MAC_COPY (pCmd->addr, pMacAddr);

    if (uKeySize > MAX_KEY_SIZE)
    {
        os_memoryCopy (pCmdBld->hOs, (void *)pCmd->key, (void *)pKey, MAX_KEY_SIZE);
    }
    else 
    {
        os_memoryCopy (pCmdBld->hOs, (void *)pCmd->key, (void *)pKey, uKeySize);
    }

    pCmd->action = ENDIAN_HANDLE_WORD((TI_UINT16)action);
    pCmd->keySize = (TI_UINT8)uKeySize;
    pCmd->type = (TI_UINT8)uKeyType;
    pCmd->id = (TI_UINT8)uKeyId;
    pCmd->ssidProfile = 0;

    /* 
     * Preserve TKIP/AES security sequence number after recovery.
     * If not in reconfig set to 0 so the FW will ignore it and keep its own number.
     * Note that our STA Tx is currently using only one sequence-counter 
     * for all ACs (unlike the Rx which is separated per AC).  
     */
    if (pCmdBld->bReconfigInProgress == TI_FALSE)
    {
        uSecuritySeqNumLow = uSecuritySeqNumHigh = 0;
    }
    pCmd->AcSeqNum16[0] = ENDIAN_HANDLE_WORD((TI_UINT16)uSecuritySeqNumLow);
    pCmd->AcSeqNum16[1] = 0;
    pCmd->AcSeqNum16[2] = 0;
    pCmd->AcSeqNum16[3] = 0;
    
    pCmd->AcSeqNum32[0] = ENDIAN_HANDLE_LONG(uSecuritySeqNumHigh);
    pCmd->AcSeqNum32[1] = 0;
    pCmd->AcSeqNum32[2] = 0;
    pCmd->AcSeqNum32[3] = 0;


TRACE6(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "Addr: %02x:%02x:%02x:%02x:%02x:%02x\n", pCmd->addr[0],pCmd->addr[1],pCmd->addr[2],pCmd->addr[3],pCmd->addr[4],pCmd->addr[5]);
			
TRACE7(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "Action=%x,keySize=0x%x,type=%x, id=%x, ssidProfile=%x, AcSeqNum16[0]=%x, AcSeqNum32[0]=%x\n", pCmd->action,pCmd->keySize, pCmd->type,pCmd->id,pCmd->ssidProfile,pCmd->AcSeqNum16[0],pCmd->AcSeqNum32[0] );

	return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_SET_KEYS, (char *)pCmd, sizeof(*pCmd), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeStartScan ()
 ****************************************************************************
 * DESCRIPTION: Send SCAN Command
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeStartScan (TI_HANDLE hCmdBld, ScanParameters_t* pScanParams, void *fScanResponseCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue,
                             CMD_SCAN, 
                             (TI_CHAR *)pScanParams,  
                             sizeof(ScanParameters_t),
                             fScanResponseCb, 
                             hCb, 
                             NULL);
}

/****************************************************************************
 *                      cmdBld_CmdIeStartSPSScan ()
 ****************************************************************************
 * DESCRIPTION: Send SPS SCAN Command
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeStartSPSScan (TI_HANDLE hCmdBld, ScheduledScanParameters_t* pScanParams, void* fScanResponseCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_SPS_SCAN, 
                             (TI_CHAR *)pScanParams, 
                             sizeof(ScheduledScanParameters_t),
                             fScanResponseCb, 
                             hCb, 
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeStopScan ()
 ****************************************************************************
 * DESCRIPTION: Construct the STOP_SCAN command fields and send it to the
 *              mailbox 
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeStopScan (TI_HANDLE hCmdBld, void *fScanResponseCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CmdIeStopScan: -------------- \n");

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_STOP_SCAN, 0, 0, fScanResponseCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeStopSPSScan ()
 ****************************************************************************
 * DESCRIPTION: Construct the STOP_SPS_SCAN command fields and send it to the
 *              mailbox 
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeStopSPSScan (TI_HANDLE hCmdBld, void* fScanResponseCB, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CmdIeStopSPSScan: -------------- \n");

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_STOP_SPS_SCAN, 0, 0, fScanResponseCB, hCb, NULL);
}


TI_STATUS cmdBld_CmdIeSetSplitScanTimeOut (TI_HANDLE hCmdBld, TI_UINT32 uTimeOut, void *fCB, TI_HANDLE hCb)
{
	TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
	enhancedTriggerTO_t Cmd_enhancedTrigger;
	enhancedTriggerTO_t *pCmd = &Cmd_enhancedTrigger;

TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_CmdIeSetSplitScanTimeOut: uTimeOut=%d -------------- \n", uTimeOut);

	pCmd->slicedScanTimeOut = uTimeOut;

	return cmdQueue_SendCommand(pCmdBld->hCmdQueue, CMD_TRIGGER_SCAN_TO, (char *)pCmd, sizeof(*pCmd), fCB, hCb, NULL);
}

/** 
 * \fn     cmdBld_CmdIeScanSsidList 
 * \brief  Sets SSID list for periodic scan 
 * 
 * Sets SSID list for periodic scan 
 * 
 * \param  hCmdBld - handle to command builder object
 * \param  pSsidList - command data
 * \param  fScanResponseCB - command complete function callback
 * \param  hCb - command complete callback handle
 * \return TI_OK on success, any other code on error 
 * \sa     cmdBld_CmdIePeriodicScanParams, cmdBld_CmdIeStartPeriodicScan, cmdBld_CmdIeStopPeriodicScan
 */ 
TI_STATUS cmdBld_CmdIeScanSsidList (TI_HANDLE hCmdBld, ConnScanSSIDList_t *pSsidList,
                                    void* fScanResponseCB, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue,
                             CMD_CONNECTION_SCAN_SSID_CFG, 
                             (char *)pSsidList,  
                             sizeof(ConnScanSSIDList_t),
                             fScanResponseCB, 
                             hCb, 
                             NULL);
}

/** 
 * \fn     cmdBld_CmdIePeriodicScanParams 
 * \brief  Sets periodic scan parameters 
 * 
 * Sets periodic scan parameters
 * 
 * \param  hCmdBld - handle to command builder object
 * \param  pPeriodicScanParams - command data
 * \param  fScanResponseCB - command complete function callback
 * \param  hCb - command complete callback handle
 * \return TI_OK on success, any other code on error 
 * \sa     cmdBld_CmdIeScanSsidList, cmdBld_CmdIeStartPeriodicScan, cmdBld_CmdIeStopPeriodicScan
 */ 
TI_STATUS cmdBld_CmdIePeriodicScanParams (TI_HANDLE hCmdBld, ConnScanParameters_t *pPeriodicScanParams,
                                          void* fScanResponseCB, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue,
                             CMD_CONNECTION_SCAN_CFG, 
                             (char *)pPeriodicScanParams,  
                             sizeof(ConnScanParameters_t),
                             fScanResponseCB, 
                             hCb, 
                             NULL);
}

/** 
 * \fn     cmdBld_CmdIeStartPeriodicScan 
 * \brief  Starts a periodic scan operation
 * 
 * Starts a periodic scan operation
 * 
 * \param  hCmdBld - handle to command builder object
 * \param  pPeriodicScanStart - command data
 * \param  fScanResponseCB - command complete function callback
 * \param  hCb - command complete callback handle
 * \return TI_OK on success, any other code on error 
 * \sa     cmdBld_CmdIeScanSsidList,  cmdBld_CmdIePeriodicScanParams, cmdBld_CmdIeStopPeriodicScan
 */ 
TI_STATUS cmdBld_CmdIeStartPeriodicScan (TI_HANDLE hCmdBld, PeriodicScanTag* pPeriodicScanStart,
                                         void* fScanResponseCB, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue,
                             CMD_START_PERIODIC_SCAN, 
                             pPeriodicScanStart, sizeof (PeriodicScanTag),
                             fScanResponseCB, 
                             hCb, 
                             NULL);
}

/** 
 * \fn     cmdBld_CmdIeStopPeriodicScan
 * \brief  Stops an on-going periodic scan operation 
 * 
 * Stops an on-going periodic scan operation
 * 
 * \param  hCmdBld - handle to command builder object
 * \param  fScanResponseCB - command complete function callback
 * \param  hCb - command complete callback handle
 * \return TI_OK on success, any other code on error 
 * \sa     cmdBld_CmdIeScanSsidList, cmdBld_CmdIePeriodicScanParams, cmdBld_CmdIeStartPeriodicScan
 */ 
TI_STATUS cmdBld_CmdIeStopPeriodicScan (TI_HANDLE hCmdBld, 
                                        PeriodicScanTag* pPeriodicScanStop, 
                                        void* fScanResponseCB, 
                                        TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue,
                             CMD_STOP_PERIODIC_SCAN, 
                             pPeriodicScanStop, 
                             sizeof(pPeriodicScanStop),
                             fScanResponseCB, 
                             hCb, 
                             NULL);
}

/****************************************************************************
 *                      cmdBld_CmdIeNoiseHistogram ()
 ****************************************************************************
 * DESCRIPTION: Send NOISE_HISTOGRAM Command
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeNoiseHistogram (TI_HANDLE hCmdBld, TNoiseHistogram *pNoiseHistParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    NoiseHistRequest_t AcxCmd_NoiseHistogram;
    NoiseHistRequest_t *pCmd = &AcxCmd_NoiseHistogram;

    os_memoryZero(pCmdBld->hOs, (void *)pCmd, sizeof(*pCmd));

    pCmd->mode = ENDIAN_HANDLE_WORD((TI_UINT16)pNoiseHistParams->cmd);
    pCmd->sampleIntervalUSec = ENDIAN_HANDLE_WORD(pNoiseHistParams->sampleInterval);

    os_memoryCopy (pCmdBld->hOs, (void *)&(pCmd->thresholds[0]), (void *)&(pNoiseHistParams->ranges[0]), MEASUREMENT_NOISE_HISTOGRAM_NUM_OF_RANGES);

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_NOISE_HIST, (TI_CHAR *)pCmd, sizeof(*pCmd), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeSetPsMode()
 ****************************************************************************
 * DESCRIPTION: send Command for Power Management configuration 
 *              to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeSetPsMode (TI_HANDLE hCmdBld, TPowerSaveParams* powerSaveParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    PSModeParameters_t   Cmd_PowerMgmtCnf;
    PSModeParameters_t * pCmd = &Cmd_PowerMgmtCnf;

    os_memoryZero(pCmdBld->hOs, (void *)pCmd, sizeof(*pCmd));

    if (powerSaveParams->ps802_11Enable)
    {
        pCmd->mode = 1;
    }
    else
    {
        pCmd->mode = 0;
    }
    
    pCmd->hangOverPeriod            = powerSaveParams->hangOverPeriod;
    pCmd->needToSendNullData        = powerSaveParams->needToSendNullData;
    pCmd->rateToTransmitNullData    = ENDIAN_HANDLE_LONG(powerSaveParams->NullPktRateModulation);
    pCmd->numberOfRetries           = powerSaveParams->numNullPktRetries;

  	return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_SET_PS_MODE, (TI_CHAR *)pCmd, sizeof(*pCmd), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeSwitchChannel ()
 ****************************************************************************
 * DESCRIPTION: Send CMD_SWITCH_CHANNEL Command
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeSwitchChannel (TI_HANDLE hCmdBld, TSwitchChannelParams *pSwitchChannelCmd, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ChannelSwitchParameters_t AcxCmd_SwitchChannel;
    ChannelSwitchParameters_t *pCmd = &AcxCmd_SwitchChannel;

    os_memoryZero (pCmdBld->hOs, (void *)pCmd, sizeof(*pCmd));

    pCmd->channel = pSwitchChannelCmd->channelNumber;
    pCmd->switchTime = pSwitchChannelCmd->switchTime;
    pCmd->txSuspend = pSwitchChannelCmd->txFlag;
    pCmd->flush = pSwitchChannelCmd->flush;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_CHANNEL_SWITCH, (TI_CHAR *)pCmd, sizeof(*pCmd), fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeSwitchChannelCancel ()
 ****************************************************************************
 * DESCRIPTION: Send CMD_SWITCH_CHANNEL_CANCEL Command
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeSwitchChannelCancel (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_STOP_CHANNEL_SWICTH, 0, 0, fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeFwDisconnect()
 ****************************************************************************
 * DESCRIPTION: Construct the Disconnect command fileds and send it to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeFwDisconnect (TI_HANDLE hCmdBld, TI_UINT32 uConfigOptions, TI_UINT32 uFilterOptions, DisconnectType_e uDisconType, TI_UINT16 uDisconReason, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    DisconnectParameters_t AcxCmd_Disconnect;
    
    AcxCmd_Disconnect.rxFilter.ConfigOptions = ENDIAN_HANDLE_LONG(uConfigOptions);
    AcxCmd_Disconnect.rxFilter.FilterOptions = ENDIAN_HANDLE_LONG(uFilterOptions);
    AcxCmd_Disconnect.disconnectReason = ENDIAN_HANDLE_LONG(uDisconReason);
    AcxCmd_Disconnect.disconnectType = uDisconType;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_DISCONNECT, 
                             (void *)&AcxCmd_Disconnect, 
                             sizeof(AcxCmd_Disconnect),
                             fCb,
                             hCb,
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeMeasurement()
 ****************************************************************************
 * DESCRIPTION: send Command for measurement configuration 
 *              to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeMeasurement (TI_HANDLE          hCmdBld, 
                                   TMeasurementParams *pMeasurementParams,
                                   void               *fMeasureResponseCb, 
                                   TI_HANDLE          hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    MeasurementParameters_t Cmd_MeasurementParam;
    MeasurementParameters_t *pCmd = &Cmd_MeasurementParam;

    os_memoryZero (pCmdBld->hOs, (void *)pCmd, sizeof(*pCmd));
    
    pCmd->band =                    pMeasurementParams->band;
    pCmd->channel =                 pMeasurementParams->channel;
    pCmd->duration =                ENDIAN_HANDLE_LONG(pMeasurementParams->duration);
    pCmd->rxFilter.ConfigOptions =  ENDIAN_HANDLE_LONG(pMeasurementParams->ConfigOptions);
    pCmd->rxFilter.FilterOptions =  ENDIAN_HANDLE_LONG(pMeasurementParams->FilterOptions);
    pCmd->scanTag =                 (TI_UINT8)pMeasurementParams->eTag;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_MEASUREMENT, 
                             (TI_CHAR *)pCmd, 
                             sizeof(*pCmd),
                             fMeasureResponseCb, 
                             hCb, 
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeMeasurementStop()
 ****************************************************************************
 * DESCRIPTION: send Command for stoping measurement  
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeMeasurementStop (TI_HANDLE hCmdBld, void* fMeasureResponseCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_STOP_MEASUREMENT, 
                             0, 
                             0,
                             fMeasureResponseCb, 
                             hCb, 
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeApDiscovery()
 ****************************************************************************
 * DESCRIPTION: send Command for AP Discovery 
 *              to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeApDiscovery (TI_HANDLE hCmdBld, TApDiscoveryParams *pApDiscoveryParams, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    ApDiscoveryParameters_t Cmd_ApDiscovery;
    ApDiscoveryParameters_t *pCmd = &Cmd_ApDiscovery;

    os_memoryZero (pCmdBld->hOs, (void *)pCmd, sizeof(*pCmd));
    
    pCmd->txPowerAttenuation = pApDiscoveryParams->txPowerDbm;
    pCmd->numOfProbRqst = pApDiscoveryParams->numOfProbRqst;
    pCmd->scanDuration  =  ENDIAN_HANDLE_LONG(pApDiscoveryParams->scanDuration);
    pCmd->scanOptions   =  ENDIAN_HANDLE_WORD(pApDiscoveryParams->scanOptions);
    pCmd->txdRateSet    =  ENDIAN_HANDLE_LONG(pApDiscoveryParams->txdRateSet);
    pCmd->rxFilter.ConfigOptions =  ENDIAN_HANDLE_LONG(pApDiscoveryParams->ConfigOptions);
    pCmd->rxFilter.FilterOptions =  ENDIAN_HANDLE_LONG(pApDiscoveryParams->FilterOptions);

	return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_AP_DISCOVERY, 
                             (void *)pCmd, 
                             sizeof(*pCmd),
                             fCb,
                             hCb,
                             NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeApDiscoveryStop()
 ****************************************************************************
 * DESCRIPTION: send Command for stoping AP Discovery
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeApDiscoveryStop (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_STOP_AP_DISCOVERY, 0, 0, fCb, hCb, NULL);
}


/****************************************************************************
 *                      cmdBld_CmdIeHealthCheck()
 ****************************************************************************
 * DESCRIPTION: 
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS:
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeHealthCheck (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, CMD_HEALTH_CHECK, NULL, 0, fCb, hCb, NULL);
}

/****************************************************************************
 *                      cmdBld_CmdIeSetStaState()
 ****************************************************************************
 * DESCRIPTION: Construct the Disconnect command fileds and send it to the mailbox
 * 
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeSetStaState (TI_HANDLE hCmdBld, TI_UINT8 staState, void *fCb, TI_HANDLE hCb)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    SetStaState_t AcxCmd_SetStaState;
    
    AcxCmd_SetStaState.staState = staState;

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_SET_STA_STATE, 
                             (void *)&AcxCmd_SetStaState, 
                             sizeof(AcxCmd_SetStaState),
                             fCb,
                             hCb,
                             NULL);
}

/****************************************************************************
 *                      cmdBld_BitIeTestCmd()
 ****************************************************************************
 * DESCRIPTION:   
 * INPUTS: None 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_CmdIeTest (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, TTestCmd* pTestCmd)
{
    TCmdBld		 *pCmdBld = (TI_HANDLE)hCmdBld;
    TI_UINT32	 paramLength;
    TI_BOOL      bIsCBfuncNecessary = TI_TRUE;

    if (NULL == pTestCmd)
    {
         TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, " pTestCmd_Buf = NULL!!!\n");
		 return TI_NOK;
    }

	if ( (TestCmdID_enum)pTestCmd->testCmdId < MAX_TEST_CMD_ID )
	{
        bIsCBfuncNecessary = TI_TRUE; 
	}
	else
	{
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_WARNING, " Unsupported testCmdId (%d)\n", pTestCmd->testCmdId);
	}

    if (bIsCBfuncNecessary && fCb == NULL) 
    {
        return TI_OK;
    }
	
	switch( pTestCmd->testCmdId )
	{
		case TEST_CMD_PD_BUFFER_CAL:
			paramLength = sizeof(TTestCmdPdBufferCal);
			break;

		case TEST_CMD_P2G_CAL:
			paramLength = sizeof(TTestCmdP2GCal);
			break;

		case TEST_CMD_RX_STAT_GET:
			paramLength = sizeof(RadioRxStatistics);
			break;

		/* packet */
		case TEST_CMD_FCC:
			paramLength = sizeof(TPacketParam);
			break;

		/* tone */
		case TEST_CMD_TELEC:
			paramLength = sizeof(TToneParam);
			break;

		case TEST_CMD_PLT_TEMPLATE:
			paramLength = sizeof(TTxTemplate);
			break;

		/* channel tune */
		case TEST_CMD_CHANNEL_TUNE:
			paramLength = sizeof(TTestCmdChannel);
			break;

		case TEST_CMD_GET_FW_VERSIONS:
			paramLength = sizeof(TFWVerisons);
			break;

        case TEST_CMD_INI_FILE_RADIO_PARAM:
            paramLength = sizeof(IniFileRadioParam);
            break;
          
        case TEST_CMD_INI_FILE_GENERAL_PARAM:
            paramLength = sizeof(IniFileGeneralParam);
            break;
          
		case TEST_CMD_PLT_GAIN_ADJUST:
			paramLength = sizeof(uint32);
			break;

		case TEST_CMD_RUN_CALIBRATION_TYPE:
			paramLength = sizeof(TTestCmdRunCalibration);
			break;

		case TEST_CMD_TX_GAIN_ADJUST:
			paramLength = sizeof(TTxGainAdjust);
			break;
        case TEST_CMD_TEST_TONE:
            paramLength = sizeof(TestToneParams_t);
            break;

		case TEST_CMD_SET_EFUSE:
			paramLength = sizeof(EfuseParameters_t);
			break;
		case TEST_CMD_GET_EFUSE:
			paramLength = sizeof(EfuseParameters_t);
			break;

		case TEST_CMD_RX_PLT_CAL:
			paramLength = sizeof(RadioRxPltCal);
			break;
			
		case TEST_CMD_UPDATE_PD_REFERENCE_POINT:
			paramLength = sizeof(TTestCmdUpdateReferncePoint);
			break;

		case TEST_CMD_UPDATE_PD_BUFFER_ERRORS:
			paramLength = sizeof(TTestCmdPdBufferErrors);
			break;
			
		case TEST_CMD_POWER_MODE:
			paramLength = sizeof(TTestCmdPowerMode);
			break;

		case TEST_CMD_STOP_TX:
		case TEST_CMD_RX_STAT_STOP:
		case TEST_CMD_RX_STAT_START:
		case TEST_CMD_RX_STAT_RESET:
		case TEST_CMD_RX_PLT_ENTER:
		case TEST_CMD_RX_PLT_EXIT:
			paramLength = 0;
			break;

		default:
			paramLength = sizeof(pTestCmd->testCmd_u);
	}

    return cmdQueue_SendCommand (pCmdBld->hCmdQueue, 
                             CMD_TEST, 
                             (void *)pTestCmd, 
                             paramLength + RESEARVED_SIZE_FOR_RESPONSE,
                             fCb, 
                             hCb, 
                             (void*)pTestCmd);    
}
