/*
 * CmdBld.c
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


/** \file  CmdBld.c 
 *  \brief Command builder main
 *
 *  \see   CmdBld.h 
 */
#define __FILE_ID__  FILE_ID_90
#include "TWDriver.h"
#include "osApi.h"
#include "tidef.h"
#include "report.h"
#include "public_infoele.h"
#include "CmdBld.h"
#include "txResult_api.h"
#include "CmdBldCmdIE.h"
#include "CmdBldCfgIE.h"
#include "CmdBldItrIE.h"
#include "CmdQueue_api.h"
#include "eventMbox_api.h"
#include "TWDriverInternal.h"
#include "HwInit_api.h"

#define DEFAULT_TRACE_ENABLE                   0
#define DEFAULT_TRACE_OUT                      0

#define DEFAULT_PBCC_DYNAMIC_ENABLE_VAL        0
#define DEFAULT_PBCC_DYNAMIC_INTERVAL          500
#define DEFAULT_PBCC_DYNAMIC_IGNORE_MCAST      0

#define DEFAULT_HW_RADIO_CHANNEL               11

#define DEFAULT_CW_MIN                         15

#define DEFAULT_USE_DEVICE_ERROR_INTERRUPT     1

#define DEFAULT_UCAST_PRIORITY                 0

#define DEFAULT_NUM_STATIONS                   1

/* only for AP */
/*  8 increase number of BC frames */
#define DEFAULT_NUM_BCAST_TX_DESC              16  

#define DEFAULT_BCAST_PRIORITY                 0x81

/* hw access method*/
typedef enum
{
    HW_ACCESS_BUS_SLAVE_INDIRECT    = 0,
    HW_ACCESS_BUS_SLAVE_DIRECT      = 1,
    HW_ACCESS_BUS_MASTER            = 2

} EHwAccessMethod;

typedef int (*TConfigFwCb) (TI_HANDLE, TI_STATUS);


static TI_STATUS cmdBld_ConfigSeq               (TI_HANDLE hCmdBld);
static TI_STATUS cmdBld_GetCurrentAssociationId (TI_HANDLE hCmdBld, TI_UINT16 *pAidVal);
static TI_STATUS cmdBld_GetArpIpAddressesTable  (TI_HANDLE hCmdBld, TIpAddr *pIpAddr, TI_UINT8 *pEnabled , EIpVer *pIpVer);
static TI_STATUS cmdBld_JoinCmpltForReconfigCb  (TI_HANDLE hCmdBld);
static TI_STATUS cmdBld_DummyCb                 (TI_HANDLE hCmdBld);




TI_HANDLE cmdBld_Create (TI_HANDLE hOs)
{
    TCmdBld        *pCmdBld;
    TI_UINT32      uNumOfStations;
    TI_UINT32      i;

    /* Allocate the command builder */
    pCmdBld = (TCmdBld *)os_memoryAlloc (hOs, sizeof(TCmdBld));
    if (pCmdBld == NULL)
    {
        WLAN_OS_REPORT(("cmdBld_Create: Error memory allocation\n"));
        return NULL;
    }
    os_memoryZero (hOs, (void *)pCmdBld, sizeof(TCmdBld));

    pCmdBld->hOs = hOs;

    /* Create the Params object */
    /* make this code flat, move it to configure */
    {
        TWlanParams     *pWlanParams    = &DB_WLAN(pCmdBld);
        TDmaParams      *pDmaParams     = &DB_DMA(pCmdBld);
        TBssInfoParams  *pBssInfoParams = &DB_BSS(pCmdBld);
        TGenCounters    *pGenCounters   = &DB_CNT(pCmdBld);

        /* General counters */
        pGenCounters->FcsErrCnt                     = 0;

        /* BSS info paramaters */
        pBssInfoParams->RadioChannel                = DEFAULT_HW_RADIO_CHANNEL;
        pBssInfoParams->Ctrl                        = 0;
        /*
         * Intilaize the ctrl field in the BSS join structure 
         * Only bit_7 in the ctrl field is vurrently in use.
         * If bit_7 is on => Doing Tx flash before joining new AP 
         */
        pBssInfoParams->Ctrl                        |= JOIN_CMD_CTRL_TX_FLUSH;

        /* WLAN parameters*/

        /* Init filters as station (start/join with BssType will overwrite the values) */
        cmdBld_SetRxFilter ((TI_HANDLE)pCmdBld, RX_CONFIG_OPTION_MY_DST_MY_BSS, RX_FILTER_OPTION_FILTER_ALL);
        pWlanParams->UseDeviceErrorInterrupt        = DEFAULT_USE_DEVICE_ERROR_INTERRUPT;
        /* Initialize the params object database fields */
        pWlanParams->hwAccessMethod = HW_ACCESS_BUS_SLAVE_INDIRECT;
        pWlanParams->maxSitesFragCollect = TWD_SITE_FRAG_COLLECT_DEF;
        pWlanParams->RtsThreshold = TWD_RTS_THRESHOLD_DEF; 
        pWlanParams->bJoin = TI_FALSE;
        /* Soft Gemini defaults */
        pWlanParams->SoftGeminiEnable = SG_DISABLE;
        /* Beacon filter defaults */
        pWlanParams->beaconFilterParams.desiredState    = TI_FALSE;
        pWlanParams->beaconFilterParams.numOfElements   = DEF_NUM_STORED_FILTERS;
        pWlanParams->beaconFilterIETable.numberOfIEs    = DEF_BEACON_FILTER_IE_TABLE_NUM;
        pWlanParams->beaconFilterIETable.IETableSize    = BEACON_FILTER_IE_TABLE_DEF_SIZE;
        /* Roaming  parameters */
        pWlanParams->roamTriggers.BssLossTimeout    = NO_BEACON_DEFAULT_TIMEOUT;
        pWlanParams->roamTriggers.TsfMissThreshold  = OUT_OF_SYNC_DEFAULT_THRESHOLD;
        /* CoexActivity table */
        pWlanParams->tWlanParamsCoexActivityTable.numOfElements = COEX_ACTIVITY_TABLE_DEF_NUM; 

        /* DMA parameters */
        /* Initialize the Params object database fields*/
        pDmaParams->NumStations                     = DEFAULT_NUM_STATIONS;
        uNumOfStations                              = (TI_UINT32)pDmaParams->NumStations;
        /* 
         * loop an all rssi_snr triggers and initialize only index number. 
         * Reason: 'index' not initialized --> 'index = 0' --> triggers 1..7 will overrun trigger '0' in cmdBld_ConfigSeq
         */
        for (i = 0; i < NUM_OF_RSSI_SNR_TRIGGERS ; i++) 
        {
           pWlanParams->tRssiSnrTrigger[i].index = i;
        }
    }
    
    pCmdBld->uLastElpCtrlMode = ELPCTRL_MODE_NORMAL;
    
    /* Create security objects */
    pCmdBld->tSecurity.eSecurityMode = TWD_CIPHER_NONE;
    pCmdBld->tSecurity.uNumOfStations = uNumOfStations;
    DB_KEYS(pCmdBld).pReconfKeys = (TSecurityKeys*)os_memoryAlloc (hOs, 
                                        sizeof(TSecurityKeys) * (uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION + NO_OF_EXTRA_RECONF_SECUR_KEYS));
    os_memoryZero (hOs, 
                   (void *)(DB_KEYS(pCmdBld).pReconfKeys), 
                   sizeof(TSecurityKeys) * (uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION + NO_OF_EXTRA_RECONF_SECUR_KEYS));


    WLAN_INIT_REPORT(("cmdBld_Create end %x\n",(TI_HANDLE)pCmdBld));

    return (TI_HANDLE)pCmdBld;
}


TI_STATUS cmdBld_Destroy (TI_HANDLE hCmdBld)
{
    TCmdBld      *pCmdBld = (TCmdBld *)hCmdBld;
    TDmaParams   *pDmaParams = NULL;
    TI_UINT32        uNumOfStations;
    
    if (pCmdBld == NULL)
    {
        return TI_OK;
    }

    pDmaParams = &DB_DMA(hCmdBld);

    uNumOfStations = (TI_UINT32)pDmaParams->NumStations;

    if (DB_KEYS(pCmdBld).pReconfKeys)
    {
        os_memoryFree (pCmdBld->hOs, 
                       DB_KEYS(pCmdBld).pReconfKeys, 
                       sizeof(TSecurityKeys) * (uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION + NO_OF_EXTRA_RECONF_SECUR_KEYS));
    }
  
    /* free the whalCtrl data structure */
    os_memoryFree (pCmdBld->hOs, pCmdBld, sizeof(TCmdBld));
    
    return TI_OK;
}

TI_STATUS cmdBld_Restart (TI_HANDLE hCmdBld)
{
    TCmdBld      *pCmdBld = (TCmdBld *)hCmdBld;
    
    /* This init is for recovery stage */
    pCmdBld->uLastElpCtrlMode = ELPCTRL_MODE_NORMAL;

    /* 
     * This call is to have the recovery process in AWAKE mode 
     * Prevent move to sleep mode between Hw_Init and Fw_Init
     */
    cmdBld_CfgIeSleepAuth (hCmdBld, DB_WLAN(hCmdBld).minPowerLevel, NULL, NULL);

    return TI_OK;
}

TI_STATUS cmdBld_Config (TI_HANDLE  hCmdBld, 
                         TI_HANDLE  hReport, 
                         void      *fFinalizeDownload, 
                         TI_HANDLE  hFinalizeDownload, 
                         TI_HANDLE  hEventMbox, 
                         TI_HANDLE  hCmdQueue,
                         TI_HANDLE  hTwIf)
{
    TCmdBld        *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32       index;
    
    pCmdBld->hReport = hReport;
    pCmdBld->fFinalizeDownload = fFinalizeDownload;
    pCmdBld->hFinalizeDownload = hFinalizeDownload;
    pCmdBld->hEventMbox = hEventMbox;
    pCmdBld->hCmdQueue = hCmdQueue;
    pCmdBld->hTwIf  = hTwIf;

    /* Reset all reconfig valid fields*/
    DB_KEYS(pCmdBld).bHwEncDecrEnableValid = TI_FALSE;
    DB_KEYS(pCmdBld).bDefaultKeyIdValid    = TI_FALSE;  
    for (index = 0; 
         index < pCmdBld->tSecurity.uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION + NO_OF_EXTRA_RECONF_SECUR_KEYS; 
         index++)
        (DB_KEYS(pCmdBld).pReconfKeys + index)->keyType = KEY_NULL;


    return TI_OK;
}



static void cmdBld_ConfigFwCb (TI_HANDLE hCmdBld, TI_STATUS status, void *pData)
{
    TCmdBld        *pCmdBld = (TCmdBld *)hCmdBld;
    MemoryMap_t    *pMemMap = &pCmdBld->tMemMap;
    TDmaParams     *pDmaParams = &DB_DMA(hCmdBld);

    /* Arrived from callback */
    if (pData)
    {
        TI_UINT32         *pSwap, i, uMemMapNumFields;

        /* Solve endian problem (all fields are 32 bit) */
        uMemMapNumFields = (sizeof(MemoryMap_t) - sizeof(EleHdrStruct)) % 4;
        pSwap = (TI_UINT32* )&(pMemMap->codeStart);
        for (i = 0; i < uMemMapNumFields; i++)
        {
            pSwap[i] = ENDIAN_HANDLE_LONG(pSwap[i]);
        }
    }

    /* Save number of TX blocks */
    pDmaParams->NumTxBlocks = pMemMap->numTxMemBlks;
    /* Firmware Control block is internally pointing to TxResultInterface structure */
    pDmaParams->fwTxResultInterface = pMemMap->trqBlock.controlBlock; 
    pDmaParams->fwRxCBufPtr =           pMemMap->rxCBufPtr;
    pDmaParams->fwTxCBufPtr =           pMemMap->txCBufPtr;
    pDmaParams->fwRxControlPtr =        pMemMap->rxControlPtr;
    pDmaParams->fwTxControlPtr =        pMemMap->txControlPtr;

    pDmaParams->PacketMemoryPoolStart = (TI_UINT32)pMemMap->packetMemoryPoolStart;

    /* Indicate that the reconfig process is over. */
    pCmdBld->bReconfigInProgress = TI_FALSE;

    /* Call the upper layer callback */
    (*((TConfigFwCb)pCmdBld->fConfigFwCb)) (pCmdBld->hConfigFwCb, TI_OK);
}


/****************************************************************************
 *                      cmdBld_ConfigFw()
 ****************************************************************************
 * DESCRIPTION: Configure the WLAN firmware
 * 
 * INPUTS: None 
 * 
 * OUTPUT: None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_ConfigFw (TI_HANDLE hCmdBld, void *fConfigFwCb, TI_HANDLE hConfigFwCb)
{
    TCmdBld        *pCmdBld = (TCmdBld *)hCmdBld;

    pCmdBld->fConfigFwCb = fConfigFwCb;
    pCmdBld->hConfigFwCb = hConfigFwCb; 
    pCmdBld->uIniSeq = 0;
    pCmdBld->bReconfigInProgress = TI_TRUE;
    /* should be re-initialized for recovery,   pCmdBld->uLastElpCtrlMode = ELPCTRL_MODE_KEEP_AWAKE; */

    /* Start configuration sequence */
    return cmdBld_ConfigSeq (hCmdBld);
}


typedef TI_STATUS (*TCmdCfgFunc) (TI_HANDLE);


static TI_STATUS __cmd_probe_req (TI_HANDLE hCmdBld)
{
    TI_STATUS   tStatus = TI_OK;

    /* keep space for 2.4 GHz probe request */
    tStatus = cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                  NULL, 
                                                  DB_WLAN(hCmdBld).probeRequestTemplateSize,
                                                  CFG_TEMPLATE_PROBE_REQ_2_4,
                                                  0,
                                                  NULL,
                                                  NULL);
    if (TI_OK != tStatus)
    {
        return tStatus;
    }

    /* keep space for 5.0 GHz probe request */
    return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                               NULL, 
                                               DB_WLAN(hCmdBld).probeRequestTemplateSize,
                                               CFG_TEMPLATE_PROBE_REQ_5,
                                               0,
                                             (void *)cmdBld_ConfigSeq,
                                             hCmdBld); 
}


static TI_STATUS __cmd_null_data (TI_HANDLE hCmdBld)
{
    return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                             NULL, 
                                             DB_WLAN(hCmdBld).nullTemplateSize,
                                               TEMPLATE_NULL_DATA,
                                               0,
                                             (void *)cmdBld_ConfigSeq,
                                             hCmdBld); 
}

static TI_STATUS __cmd_burst_mode_enable (TI_HANDLE hCmdBld)
{
	return cmdBld_CfgIeBurstMode (hCmdBld, 
							  DB_AC(hCmdBld).isBurstModeEnabled, 
							  (void *)cmdBld_ConfigSeq, 
							  hCmdBld);
}


static TI_STATUS __cmd_disconn (TI_HANDLE hCmdBld)
{
    return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                             NULL, 
                                             DB_WLAN(hCmdBld).disconnTemplateSize,
                                             TEMPLATE_DISCONNECT,
                                             0,
                                             (void *)cmdBld_ConfigSeq,
                                             hCmdBld); 
}

static TI_STATUS __cmd_ps_poll (TI_HANDLE hCmdBld)
{
    return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                             NULL, 
                                             DB_WLAN(hCmdBld).PsPollTemplateSize,
                                               TEMPLATE_PS_POLL,
                                               0,
                                             (void *)cmdBld_ConfigSeq,
                                             hCmdBld); 
}


static TI_STATUS __cmd_qos_null_data (TI_HANDLE hCmdBld)
{
    return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                             NULL, 
                                             DB_WLAN(hCmdBld).qosNullDataTemplateSize,
                                               TEMPLATE_QOS_NULL_DATA,
                                               0,
                                             (void *)cmdBld_ConfigSeq,
                                             hCmdBld); 
}


static TI_STATUS __cmd_probe_resp (TI_HANDLE hCmdBld)
{
    return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                             NULL, 
                                             DB_WLAN(hCmdBld).probeResponseTemplateSize,
                                               TEMPLATE_PROBE_RESPONSE,
                                               0,
                                             (void *)cmdBld_ConfigSeq,
                                             hCmdBld); 
}


static TI_STATUS __cmd_beacon (TI_HANDLE hCmdBld)
{
    return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                             NULL, 
                                             DB_WLAN(hCmdBld).beaconTemplateSize,
                                               TEMPLATE_BEACON,
                                               0,
                                             (void *)cmdBld_ConfigSeq,
                                             hCmdBld); 
}

static TI_STATUS __cmd_keep_alive_tmpl (TI_HANDLE hCmdBld)
{
    TI_UINT32   index;
    TI_STATUS   status = TI_NOK;

    /* 
     * config templates 
     * fisr configure all indexes but the last one with no CB, and than configure the last one 
     * with a CB to continue configuration.
     */
    for (index = 0; index < KLV_MAX_TMPL_NUM - 1; index++)
    {
        status =  cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                      NULL, 
                                                      MAX_TEMPLATES_SIZE,
                                                      TEMPLATE_KLV,
                                                      index,
                                                      NULL,
                                                      NULL);
        if (TI_OK != status)
        {
            return status;
        }
    }

    status =  cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                  NULL, 
                                                  MAX_TEMPLATES_SIZE,
                                                  TEMPLATE_KLV,
                                                  index,
                                                  (void *)cmdBld_ConfigSeq,
                                                  hCmdBld);

    return status;
}


static TI_STATUS __cfg_mem (TI_HANDLE hCmdBld)
{
    /* Configure the weight among the different hardware queues */
    return cmdBld_CfgIeConfigMemory (hCmdBld, &DB_DMA(hCmdBld), (void *)cmdBld_ConfigSeq, hCmdBld);
}


static TI_STATUS __cfg_rx_msdu_life_time (TI_HANDLE hCmdBld)
{    
    /* Configure the Rx Msdu Life Time (expiry time of de-fragmentation in FW) */
    return cmdBld_CfgIeRxMsduLifeTime (hCmdBld, 
                                       DB_WLAN(hCmdBld).MaxRxMsduLifetime, 
                                       (void *)cmdBld_ConfigSeq, 
                                       hCmdBld);
}


static TI_STATUS __cfg_rx (TI_HANDLE hCmdBld)
{    
    return cmdBld_CfgIeRx (hCmdBld, 
                           DB_WLAN(hCmdBld).RxConfigOption, 
                           DB_WLAN(hCmdBld).RxFilterOption, 
                           (void *)cmdBld_ConfigSeq, 
                           hCmdBld);
}


static TI_STATUS __cfg_ac_params_0 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */

    if (DB_AC(hCmdBld).isAcConfigured[0])
    {
        return cmdBld_CfgAcParams (hCmdBld, &DB_AC(hCmdBld).ac[0], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_ac_params_1 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */

    if (DB_AC(hCmdBld).isAcConfigured[1])
    {
        return cmdBld_CfgAcParams (hCmdBld, &DB_AC(hCmdBld).ac[1], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_ac_params_2 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */

    if (DB_AC(hCmdBld).isAcConfigured[2])
    {
        return cmdBld_CfgAcParams (hCmdBld, &DB_AC(hCmdBld).ac[2], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_ac_params_3 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */

    if (DB_AC(hCmdBld).isAcConfigured[3])
    {
        return cmdBld_CfgAcParams (hCmdBld, &DB_AC(hCmdBld).ac[3], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_tid_0 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */
    if (DB_QUEUES(hCmdBld).isQueueConfigured[0])
    {
        return cmdBld_CfgTid (hCmdBld, &DB_QUEUES(hCmdBld).queues[0], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_tid_1 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */
    if (DB_QUEUES(hCmdBld).isQueueConfigured[1])
    {
        return cmdBld_CfgTid (hCmdBld, &DB_QUEUES(hCmdBld).queues[1], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_tid_2 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */
    if (DB_QUEUES(hCmdBld).isQueueConfigured[2])
    {
        return cmdBld_CfgTid (hCmdBld, &DB_QUEUES(hCmdBld).queues[2], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_tid_3 (TI_HANDLE hCmdBld)
{
    /*
     * NOTE: Set following parameter only if configured.
     *       Otherwise, is contains garbage.
     */
    if (DB_QUEUES(hCmdBld).isQueueConfigured[3])
    {
        return cmdBld_CfgTid (hCmdBld, &DB_QUEUES(hCmdBld).queues[3], (void *)cmdBld_ConfigSeq, hCmdBld);
    }
    else
    {
        return TI_NOK;
    }
}


static TI_STATUS __cfg_ps_rx_streaming (TI_HANDLE hCmdBld)
{
    TI_UINT32       index;
    TI_STATUS        eStatus;
    TPsRxStreaming *pPsRxStreaming;


    if (!DB_WLAN(hCmdBld).bJoin)
    {
        return TI_NOK;
    }

    /* Config enabled streams (disable is the FW default). */
    for (index = 0; index < MAX_NUM_OF_802_1d_TAGS - 1; index++)
    {
        pPsRxStreaming = &(DB_PS_STREAM(hCmdBld).tid[index]);

        if (pPsRxStreaming->bEnabled)
        {
            eStatus = cmdBld_CfgPsRxStreaming (hCmdBld, pPsRxStreaming, NULL, NULL);
            if (eStatus != TI_OK) 
            {
                return eStatus;
            }
        }
    }

    /* Set NOK for a case the following config is skipped, to indicate that no callback is expected */
    eStatus = TI_NOK;

    pPsRxStreaming = &(DB_PS_STREAM(hCmdBld).tid[MAX_NUM_OF_802_1d_TAGS - 1]);
    if (pPsRxStreaming->bEnabled)
    {
        eStatus = cmdBld_CfgPsRxStreaming (hCmdBld, pPsRxStreaming, (void *)cmdBld_ConfigSeq, hCmdBld);
        if (eStatus != TI_OK) 
        {
            return eStatus;
        }
    }

    return eStatus; 
    }


static TI_STATUS __cfg_rx_data_filter (TI_HANDLE hCmdBld)
{
    TI_UINT32       index;
    TI_STATUS       eStatus;
    TRxDataFilter   *pFilters;


    if (DB_RX_DATA_FLTR(hCmdBld).bEnabled) 
    {
        eStatus = cmdBld_CfgIeEnableRxDataFilter (hCmdBld, 
                                                  DB_RX_DATA_FLTR(hCmdBld).bEnabled, 
                                                  DB_RX_DATA_FLTR(hCmdBld).eDefaultAction, 
                                                  NULL, 
                                                  NULL);
        if (eStatus != TI_OK) 
        {
            return eStatus;
        }
    }

    /* 
     * Config enabled filters (last one is separated to use the callback) 
     */
    for (index = 0; index < MAX_DATA_FILTERS - 1; index++)
    {
        pFilters = &(DB_RX_DATA_FLTR(hCmdBld).aRxDataFilter[index]);

        if (pFilters->uCommand == ADD_FILTER)
        {
            eStatus = cmdBld_CfgIeRxDataFilter (hCmdBld, 
                                                pFilters->uIndex, 
                                                pFilters->uCommand, 
                                                pFilters->eAction, 
                                                pFilters->uNumFieldPatterns, 
                                                pFilters->uLenFieldPatterns, 
                                                pFilters->aFieldPattern, 
                                                NULL, 
                                                NULL);
            if (eStatus != TI_OK) 
    {
                return eStatus;
            }
        }
    }

    /* Set NOK for a case the following config is skipped, to indicate that no callback is expected */
        eStatus = TI_NOK;

    pFilters = &(DB_RX_DATA_FLTR(hCmdBld).aRxDataFilter[MAX_DATA_FILTERS - 1]);
    if (pFilters->uCommand == ADD_FILTER)
    {
        eStatus = cmdBld_CfgIeRxDataFilter (hCmdBld, 
                                            pFilters->uIndex, 
                                            pFilters->uCommand, 
                                            pFilters->eAction, 
                                            pFilters->uNumFieldPatterns, 
                                            pFilters->uLenFieldPatterns, 
                                            pFilters->aFieldPattern, 
                                            (void *)cmdBld_ConfigSeq, 
                                            hCmdBld);
        if (eStatus != TI_OK) 
        {
            return eStatus;
        }
    }

    return eStatus;
}


static TI_STATUS __cfg_pd_threshold (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIePacketDetectionThreshold (hCmdBld, 
                                                 DB_WLAN(hCmdBld).PacketDetectionThreshold, 
                                                 (void *)cmdBld_ConfigSeq, 
                                                 hCmdBld);
}


static TI_STATUS __cfg_slot_time (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeSlotTime (hCmdBld, DB_WLAN(hCmdBld).SlotTime, (void *)cmdBld_ConfigSeq, hCmdBld);
}


static TI_STATUS __cfg_arp_ip_filter (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeArpIpFilter (hCmdBld, 
                                    DB_WLAN(hCmdBld).arp_IP_addr, 
                                    (EArpFilterType)DB_WLAN(hCmdBld).arpFilterType, 
                                    (void *)cmdBld_ConfigSeq, 
                                    hCmdBld);
}

static TI_STATUS __cfg_group_address_table (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeGroupAdressTable (hCmdBld, 
                                         DB_WLAN(hCmdBld).numGroupAddrs, 
                                         DB_WLAN(hCmdBld).aGroupAddr, 
                                         DB_WLAN(hCmdBld).isMacAddrFilteringnabled, 
                                         (void *)cmdBld_ConfigSeq, 
                                         hCmdBld);
}


static TI_STATUS __cfg_service_period_timeout (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeServicePeriodTimeout (hCmdBld, 
                                             &DB_WLAN(hCmdBld).rxTimeOut, 
                                             (void *)cmdBld_ConfigSeq, 
                                             hCmdBld);
}


static TI_STATUS __cfg_rts_threshold (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgRtsThreshold (hCmdBld, 
                                   DB_WLAN(hCmdBld).RtsThreshold, 
                                   (void *)cmdBld_ConfigSeq, 
                                   hCmdBld);
}

static TI_STATUS __cfg_dco_itrim_params (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgDcoItrimParams (hCmdBld, 
                                     DB_WLAN(hCmdBld).dcoItrimEnabled, 
                                     DB_WLAN(hCmdBld).dcoItrimModerationTimeoutUsec, 
                                     (void *)cmdBld_ConfigSeq, 
                                     hCmdBld);
}

static TI_STATUS __cfg_fragment_threshold (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeFragmentThreshold (hCmdBld, 
                                          DB_WLAN(hCmdBld).FragmentThreshold, 
                                          (void *)cmdBld_ConfigSeq, 
                                          hCmdBld);
}


static TI_STATUS __cfg_pm_config (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIePmConfig (hCmdBld, 
                                 DB_WLAN(hCmdBld).uHostClkSettlingTime,
                                 DB_WLAN(hCmdBld).uHostFastWakeupSupport, 
                                 (void *)cmdBld_ConfigSeq, 
                                 hCmdBld);
}


static TI_STATUS __cfg_beacon_filter_opt (TI_HANDLE hCmdBld)
{
    /* Set The Beacon Filter in HAL */
    return cmdBld_CfgIeBeaconFilterOpt (hCmdBld, 
                                        DB_WLAN(hCmdBld).beaconFilterParams.desiredState,
                                        DB_WLAN(hCmdBld).beaconFilterParams.numOfElements, 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_beacon_filter_table (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeBeaconFilterTable (hCmdBld, 
                                          DB_WLAN(hCmdBld).beaconFilterIETable.numberOfIEs,
                                          DB_WLAN(hCmdBld).beaconFilterIETable.IETable,
                                          DB_WLAN(hCmdBld).beaconFilterIETable.IETableSize, 
                                          (void *)cmdBld_ConfigSeq, 
                                          hCmdBld);
}


static TI_STATUS __cfg_tx_cmplt_pacing (TI_HANDLE hCmdBld)
{    
    return cmdBld_CfgIeTxCmpltPacing (hCmdBld, 
                                      DB_WLAN(hCmdBld).TxCompletePacingThreshold,
                                      DB_WLAN(hCmdBld).TxCompletePacingTimeout,
                                      (void *)cmdBld_ConfigSeq, 
                                      hCmdBld);
}


static TI_STATUS __cfg_rx_intr_pacing (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeRxIntrPacing (hCmdBld, 
                                     DB_WLAN(hCmdBld).RxIntrPacingThreshold,
                                     DB_WLAN(hCmdBld).RxIntrPacingTimeout,
                         (void *)cmdBld_ConfigSeq, 
                               hCmdBld);
}


#ifdef TI_TEST
static TI_STATUS __cfg_coex_activity_table (TI_HANDLE hCmdBld)
{
    TCmdBld       *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32 uNumberOfIEs = DB_WLAN(hCmdBld).tWlanParamsCoexActivityTable.numOfElements;
    TCoexActivity *CoexActivityTable = DB_WLAN(hCmdBld).tWlanParamsCoexActivityTable.entry;
    TI_STATUS   status = TI_NOK;
    TI_UINT32   index;

    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , " CoexActivity, uNumberOfIEs=%d\n", uNumberOfIEs);
    if (uNumberOfIEs == 0)
    {
        return status;
    }
    /* 
     * config CoexActivity table 
     * first configure all indexes but the last one with no CB, and than configure the last one 
     * with a CB to continue configuration.
     */
    for (index = 0; index < uNumberOfIEs-1; index++)
    {
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , " CoexActivity, send %d\n", index);
        status =  cmdBld_CfgIeCoexActivity (hCmdBld, &CoexActivityTable[index], 
                                                NULL, NULL);
        if (TI_OK != status)
        {
            return status;
        }
    }

    /* Send last activity with a callback to continue config sequence */
    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , " CoexActivity, send last %d\n", index);
    status =  cmdBld_CfgIeCoexActivity (hCmdBld, &CoexActivityTable[index], 
                                            (void *)cmdBld_ConfigSeq, hCmdBld);

    return status;
}
#endif

static TI_STATUS __cfg_cca_threshold (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeCcaThreshold (hCmdBld, 
                                     DB_WLAN(hCmdBld).ch14TelecCca, 
                                     (void *)cmdBld_ConfigSeq, 
                                     hCmdBld);
}


static TI_STATUS __cfg_bcn_brc_options (TI_HANDLE hCmdBld)
{
    TPowerMgmtConfig powerMgmtConfig;                   

    /* Beacon broadcast options */
    powerMgmtConfig.BcnBrcOptions = DB_WLAN(hCmdBld).BcnBrcOptions;
    powerMgmtConfig.ConsecutivePsPollDeliveryFailureThreshold = DB_WLAN(hCmdBld).ConsecutivePsPollDeliveryFailureThreshold;

    return cmdBld_CfgIeBcnBrcOptions (hCmdBld, 
                                      &powerMgmtConfig, 
                                      (void *)cmdBld_ConfigSeq, 
                                      hCmdBld);
}


static TI_STATUS __cmd_enable_rx (TI_HANDLE hCmdBld)
{
    /* Enable rx path on the hardware */
    return cmdBld_CmdEnableRx (hCmdBld, (void *)cmdBld_ConfigSeq, hCmdBld);
}


static TI_STATUS __cmd_enable_tx (TI_HANDLE hCmdBld)
{
    /* Enable tx path on the hardware */
    return cmdBld_CmdEnableTx (hCmdBld, 
                               DB_DEFAULT_CHANNEL(hCmdBld), 
                               (void *)cmdBld_ConfigSeq, 
                               hCmdBld);
}


static TI_STATUS __cfg_ps_wmm (TI_HANDLE hCmdBld)
{
    /* ACX for a work around for Wi-Fi test */
    return cmdBld_CfgIePsWmm (hCmdBld, 
                              DB_WLAN(hCmdBld).WiFiWmmPS, 
                              (void *)cmdBld_ConfigSeq, 
                              hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_weights (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Weights for Average calculations */
    return cmdBld_CfgIeRssiSnrWeights (hCmdBld, 
                                       &DB_WLAN(hCmdBld).tRssiSnrWeights, 
                                       (void *)cmdBld_ConfigSeq, 
                                       hCmdBld);
}


static TI_STATUS __cfg_event_scan_cmplt (TI_HANDLE hCmdBld)
{    
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    /* Enable the scan complete interrupt source */
    return eventMbox_UnMaskEvent (pCmdBld->hEventMbox, 
                               TWD_OWN_EVENT_SCAN_CMPLT, 
                               (void *)cmdBld_ConfigSeq, 
                               hCmdBld);
}

static TI_STATUS __cfg_event_sps_scan_cmplt (TI_HANDLE hCmdBld)
{    
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    return eventMbox_UnMaskEvent (pCmdBld->hEventMbox, 
                               TWD_OWN_EVENT_SPS_SCAN_CMPLT, 
                               (void *)cmdBld_ConfigSeq, 
                               hCmdBld);
}

static TI_STATUS __cfg_event_plt_rx_calibration_cmplt (TI_HANDLE hCmdBld)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    return eventMbox_UnMaskEvent (pCmdBld->hEventMbox, 
                               TWD_OWN_EVENT_PLT_RX_CALIBRATION_COMPLETE, 
                               (void *)cmdBld_ConfigSeq, 
                               hCmdBld);
}


static TI_STATUS __cfg_hw_enc_dec_enable (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgHwEncDecEnable (hCmdBld, TI_TRUE, (void *)cmdBld_ConfigSeq, hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_0 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[0], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_1 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[1], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_2 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[2], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_3 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[3], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_4 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[4], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_5 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[5], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_6 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[6], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_rssi_snr_trigger_7 (TI_HANDLE hCmdBld)
{
    /* RSSI/SNR Troggers */
    return  cmdBld_CfgIeRssiSnrTrigger (hCmdBld, 
                                        &DB_WLAN(hCmdBld).tRssiSnrTrigger[7], 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_max_tx_retry (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeMaxTxRetry (hCmdBld, 
                                   &DB_WLAN(hCmdBld).roamTriggers, 
                                   (void *)cmdBld_ConfigSeq, 
                                   hCmdBld);
}



static TI_STATUS __cfg_split_scan_timeout (TI_HANDLE hCmdBld)
{
    return cmdBld_CmdIeSetSplitScanTimeOut (hCmdBld, 
                                            DB_WLAN(hCmdBld).uSlicedScanTimeOut, 
                                            (void *)cmdBld_ConfigSeq, 
                                            hCmdBld);
}


static TI_STATUS __cfg_conn_monit_params (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeConnMonitParams (hCmdBld, 
                                        &DB_WLAN(hCmdBld).roamTriggers, 
                                        (void *)cmdBld_ConfigSeq, 
                                        hCmdBld);
}


static TI_STATUS __cfg_bet (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgBet (hCmdBld, 
                          DB_WLAN(hCmdBld).BetEnable, 
                          DB_WLAN(hCmdBld).MaximumConsecutiveET,
                          (void *)cmdBld_ConfigSeq, 
                          hCmdBld);
}
                

static TI_STATUS __cfg_cts_protection (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeCtsProtection (hCmdBld,
                                      DB_WLAN(hCmdBld).CtsToSelf, 
                                      (void *)cmdBld_ConfigSeq, 
                                      hCmdBld);
}


static TI_STATUS __cfg_radio_params (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeRadioParams (hCmdBld, 
                                    &DB_RADIO(hCmdBld), 
                                    (void *)cmdBld_ConfigSeq, 
                                    hCmdBld);
}


static TI_STATUS __cfg_extended_radio_params (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeExtendedRadioParams (hCmdBld,
											&DB_EXT_RADIO(hCmdBld),
											(void *)cmdBld_ConfigSeq,
											hCmdBld);
}


static TI_STATUS __cfg_platform_params (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgPlatformGenParams(hCmdBld, 
                                      &DB_GEN(hCmdBld), 
                                      (void *)cmdBld_ConfigSeq, 
                                      hCmdBld);
}



static TI_STATUS __cfg_tx_rate_policy (TI_HANDLE hCmdBld)
{
    /*
     * JOIN (use the local parameters), otherwize the CORE will reconnect
     */
    if (DB_WLAN(hCmdBld).bJoin)
    {
        /* Set TxRatePolicy */
        return cmdBld_CfgTxRatePolicy (hCmdBld, 
                                       &DB_BSS(hCmdBld).TxRateClassParams, 
                                       (void *)cmdBld_ConfigSeq, 
                                       hCmdBld);
    }

    return TI_NOK;
}


static TI_STATUS __cmd_beacon_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin && DB_TEMP(hCmdBld).Beacon.Size != 0)
    {
        return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   &(DB_TEMP(hCmdBld).Beacon), 
                                                 (TI_UINT16)DB_TEMP(hCmdBld).Beacon.Size,
                                                   TEMPLATE_BEACON,
                                                   0,
                                                 (void *)cmdBld_ConfigSeq,
                                                 hCmdBld);
    }

    return TI_NOK;
}
    
static TI_STATUS __cmd_probe_resp_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin && DB_TEMP(hCmdBld).ProbeResp.Size != 0)
    {
        return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   &(DB_TEMP(hCmdBld).ProbeResp), 
                                                 (TI_UINT16)DB_TEMP(hCmdBld).ProbeResp.Size,
                                                   TEMPLATE_PROBE_RESPONSE,
                                                   0,
                                                 (void *)cmdBld_ConfigSeq,
                                                 hCmdBld);
    }

    return TI_NOK;
}


static TI_STATUS __cmd_probe_req_join (TI_HANDLE hCmdBld)
{
    TI_STATUS   tStatus;

    /* set Probe Req template also if join == false ! */
    if (DB_TEMP(hCmdBld).ProbeReq24.Size != 0)
    {
        tStatus =  cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                       &(DB_TEMP(hCmdBld).ProbeReq24), 
                                                       (TI_UINT16)DB_TEMP(hCmdBld).ProbeReq24.Size,
                                                       CFG_TEMPLATE_PROBE_REQ_2_4,
                                                       0,
                                                       NULL,
                                                       NULL);
        if (TI_OK != tStatus)
        {
            return tStatus;
        }
    }

    /* set Probe Req template also if join == false ! */
    if (DB_TEMP(hCmdBld).ProbeReq50.Size != 0)
    {
        return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   &(DB_TEMP(hCmdBld).ProbeReq50), 
                                                   (TI_UINT16)DB_TEMP(hCmdBld).ProbeReq50.Size,
                                                   CFG_TEMPLATE_PROBE_REQ_5,
                                                   0,
                                                 (void *)cmdBld_ConfigSeq,
                                                 hCmdBld);
    }

    return TI_NOK;
}


static TI_STATUS __cmd_null_data_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin && DB_TEMP(hCmdBld).NullData.Size != 0)
    {
        return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   &(DB_TEMP(hCmdBld).NullData), 
                                                 (TI_UINT16)DB_TEMP(hCmdBld).NullData.Size,
                                                   TEMPLATE_NULL_DATA,
                                                   0,
                                                 (void *)cmdBld_ConfigSeq,
                                                 hCmdBld);
    }

    return TI_NOK;
}

static TI_STATUS __cmd_disconn_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin && DB_TEMP(hCmdBld).Disconn.Size != 0)
    {
        return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   &(DB_TEMP(hCmdBld).Disconn), 
                                                   (TI_UINT16)DB_TEMP(hCmdBld).Disconn.Size,
                                                   TEMPLATE_DISCONNECT,
                                                   0,
                                                   (void *)cmdBld_ConfigSeq,
                                                   hCmdBld);
    }

    return TI_NOK;
}

static TI_STATUS __cmd_ps_poll_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin && DB_TEMP(hCmdBld).PsPoll.Size != 0)
    {
        return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   &(DB_TEMP(hCmdBld).PsPoll), 
                                                 (TI_UINT16)DB_TEMP(hCmdBld).PsPoll.Size,
                                                   TEMPLATE_PS_POLL,
                                                   0,
                                                 (void *)cmdBld_ConfigSeq,
                                                 hCmdBld);
    }

    return TI_NOK;
}

static TI_STATUS __cmd_arp_rsp (TI_HANDLE hCmdBld)
{

   return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   NULL, 
                                                   DB_WLAN(hCmdBld).ArpRspTemplateSize,
                                                   TEMPLATE_ARP_RSP,
                                                   0,
                                                   (void *)cmdBld_ConfigSeq,
                                                   hCmdBld);
}


static TI_STATUS __cmd_arp_rsp_join (TI_HANDLE hCmdBld)
{
    
    if ((DB_WLAN(hCmdBld).bJoin) && (DB_TEMP(hCmdBld).ArpRsp.Size != 0))
    {
        return cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                   &(DB_TEMP(hCmdBld).ArpRsp), 
                                                   DB_TEMP(hCmdBld).ArpRsp.Size,
                                                   TEMPLATE_ARP_RSP,
                                                   0,
                                                   (void *)cmdBld_ConfigSeq,
                                                   hCmdBld);
    }

    return TI_NOK;
}




static TI_STATUS __cmd_keep_alive_tmpl_join (TI_HANDLE hCmdBld)
{
    TI_UINT32   index;
    TI_STATUS   status = TI_NOK;

    /* 
     * config templates 
     * first configure all indexes but the last one with no CB, and than configure the last one 
     * with a CB to continue configuration.
     */
    for (index = 0; index < KLV_MAX_TMPL_NUM - 1; index++)
    {
        if (DB_WLAN(hCmdBld).bJoin && DB_TEMP(hCmdBld).KeepAlive[ index ].Size != 0)
        {
            status =  cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                          &(DB_TEMP(hCmdBld).KeepAlive[index]), 
                                                          (TI_UINT16)DB_TEMP(hCmdBld).KeepAlive[index].Size,
                                                          TEMPLATE_KLV,
                                                          index,
                                                          NULL,
                                                          NULL);
            if (TI_OK != status)
            {
                return status;
            }
        }
    }

    if (DB_WLAN(hCmdBld).bJoin && DB_TEMP(hCmdBld).KeepAlive[ index ].Size != 0)
    {
        status =  cmdBld_CmdIeConfigureTemplateFrame (hCmdBld, 
                                                      &(DB_TEMP(hCmdBld).KeepAlive[index]), 
                                                      (TI_UINT16)DB_TEMP(hCmdBld).KeepAlive[index].Size,
                                                      TEMPLATE_KLV,
                                                      index,
                                                      (void *)cmdBld_ConfigSeq,
                                                      hCmdBld);
        if (TI_OK != status)
        {
            return status;
        }
    }

    return status;
}


static TI_STATUS __cmd_keep_alive_params(TI_HANDLE hCmdBld)
{
    TI_UINT32   index;
    TI_STATUS   status;

    /* config gloabl enable / disable flag */
    cmdBld_CfgKeepAliveEnaDis (hCmdBld, DB_KLV(hCmdBld).enaDisFlag, NULL, NULL);

    /* 
     * config per-template params 
     * fisr configure all indexes but the last one with no CB, and than configure the last one 
     * with a CB to continue configuration.
     */
    for (index = 0; index < KLV_MAX_TMPL_NUM - 1; index++)
    {
        if (DB_WLAN(hCmdBld).bJoin && DB_KLV(hCmdBld).keepAliveParams[ index ].enaDisFlag != 0)
        {
            status =  cmdBld_CmdIeConfigureKeepAliveParams (hCmdBld, 
                                                            index,
                                                            DB_KLV(hCmdBld).keepAliveParams[ index ].enaDisFlag,
                                                            DB_KLV(hCmdBld).keepAliveParams[ index ].trigType,
                                                            DB_KLV(hCmdBld).keepAliveParams[ index ].interval,
                                                            NULL,
                                                            NULL);
            if (TI_OK != status)
            {
                return status;
            }
        }
    }

    /* Set NOK for a case the following config is skipped, to indicate that no callback is expected */
    status = TI_NOK;

    if (DB_WLAN(hCmdBld).bJoin && DB_KLV(hCmdBld).keepAliveParams[ index ].enaDisFlag != 0)
    {
        status =  cmdBld_CmdIeConfigureKeepAliveParams (hCmdBld, 
                                                        index,
                                                        DB_KLV(hCmdBld).keepAliveParams[ index ].enaDisFlag,
                                                        DB_KLV(hCmdBld).keepAliveParams[ index ].trigType,
                                                        DB_KLV(hCmdBld).keepAliveParams[ index ].interval,
                                                        (void *)cmdBld_ConfigSeq,
                                                        hCmdBld);
        if (TI_OK != status)
        {
            return status;
        }
    }

    return status;
}

static TI_STATUS __cmd_power_auth (TI_HANDLE hCmdBld)
{
    return cmdBld_CfgIeSleepAuth (hCmdBld,
                              DB_WLAN(hCmdBld).minPowerLevel,
                              (void *)cmdBld_ConfigSeq,
                              hCmdBld);
}

static TI_STATUS __cmd_start_join (TI_HANDLE hCmdBld)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    if (DB_WLAN(hCmdBld).bJoin)
    {
        /* 
         * Replace the Join-Complete event CB by a local function.
         * Thus, the reconfig sequence will not continue until the Join is completed!
         * The original CB is restored after Join-Complete.
         */
        eventMbox_ReplaceEvent (pCmdBld->hEventMbox, 
                                  TWD_OWN_EVENT_JOIN_CMPLT, 
                                  (void *)cmdBld_JoinCmpltForReconfigCb, 
                                  hCmdBld,                                   
                                  &pCmdBld->fJoinCmpltOriginalCbFunc, 
                                  &pCmdBld->hJoinCmpltOriginalCbHndl);                                    
        /*
         * Call the hardware to start/join the bss 
         */
        return cmdBld_CmdStartJoin (hCmdBld, 
                                    (ScanBssType_e)DB_BSS(hCmdBld).ReqBssType, 
                                    (void *)cmdBld_DummyCb, 
                                    hCmdBld);
    }

    return TI_NOK;
}

static TI_STATUS __cmd_sta_state (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bStaConnected)
    {
        return cmdBld_CmdSetStaState (hCmdBld, 
                                    STA_STATE_CONNECTED, 
                                    (void *)cmdBld_ConfigSeq, 
                                    hCmdBld);
    }

    return TI_NOK;
}

static TI_STATUS __cfg_aid (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin)
    {
        return cmdBld_CfgAid (hCmdBld, DB_WLAN(hCmdBld).Aid, (void *)cmdBld_ConfigSeq, hCmdBld);
    }

    return TI_NOK;
}


static TI_STATUS __cfg_slot_time_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin)
    {
        /* Slot time must be setting after doing join */
        return cmdBld_CfgSlotTime (hCmdBld, (ESlotTime)(DB_WLAN(hCmdBld).SlotTime), (void *)cmdBld_ConfigSeq, hCmdBld);              
    }

    return TI_NOK;
}


static TI_STATUS __cfg_preamble_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin)
    {
        /* Preamble type must be set after doing join */
        return cmdBld_CfgPreamble (hCmdBld, (Preamble_e) DB_WLAN(hCmdBld).preamble, (void *)cmdBld_ConfigSeq, hCmdBld);              
    }

    return TI_NOK;
}


static TI_STATUS __cfg_ht_capabilities (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin && DB_BSS(hCmdBld).bHtCap)
    {
        /* HT capabilities must be set after doing join */
        return cmdBld_CfgIeSetFwHtCapabilities (hCmdBld, 
                                                DB_BSS(hCmdBld).uHtCapabilites, 
                                                DB_BSS(hCmdBld).tMacAddress,
                                                DB_BSS(hCmdBld).uAmpduMaxLeng,
                                                DB_BSS(hCmdBld).uAmpduMinSpac,
                                                (void *)cmdBld_ConfigSeq, 
                                                hCmdBld);              
    }

    return TI_NOK;
}


static TI_STATUS __cfg_ht_information (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin && DB_BSS(hCmdBld).bHtInf)
    {
        /* HT Information must be set after doing join */
        return cmdBld_CfgIeSetFwHtInformation (hCmdBld, 
                                               DB_BSS(hCmdBld).uRifsMode,
                                               DB_BSS(hCmdBld).uHtProtection,
                                               DB_BSS(hCmdBld).uGfProtection,
                                               DB_BSS(hCmdBld).uHtTxBurstLimit,
                                               DB_BSS(hCmdBld).uDualCtsProtection,
                                               (void *)cmdBld_ConfigSeq, 
                                               hCmdBld);              
    }

    return TI_NOK;
}


static TI_STATUS __cfg_ba_set_session (TI_HANDLE hCmdBld)
{
	TI_STATUS tRes = TI_NOK;

    if (DB_WLAN(hCmdBld).bJoin)
    {
        TI_UINT32 uTid;
		TI_UINT32 uLastTid = MAX_NUM_OF_802_1d_TAGS; /* initial value is "not found" */

		/* Look through configured BA sessions in data base to find the last set TID */
        for (uTid = 0; uTid < MAX_NUM_OF_802_1d_TAGS; uTid++)
        {
            /* Is BA initiator or responder configured? */
            if (DB_BSS(hCmdBld).bBaInitiator[uTid] || DB_BSS(hCmdBld).bBaResponder[uTid])
            {
				uLastTid = uTid;
            }
		}

		if (uLastTid != MAX_NUM_OF_802_1d_TAGS)
		{
			/* At least one TID is set */
			for (uTid = 0; uTid < uLastTid; ++uTid)
			{
				if (DB_BSS(hCmdBld).bBaInitiator[uTid])
				{
					/* set BA Initiator */
					tRes = cmdBld_CfgIeSetBaSession (hCmdBld,
													 ACX_BA_SESSION_INITIATOR_POLICY,
													 uTid,
													 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].uPolicy,
													 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].aMacAddress,
													 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].uWinSize,
													 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uTid].uInactivityTimeout,
													 NULL,
													 NULL);
					if (tRes != TI_OK)
					{
						return tRes;
					}
				}

				if (DB_BSS(hCmdBld).bBaResponder[uTid])
				{
					/* set BA Responder */
					tRes = cmdBld_CfgIeSetBaSession (hCmdBld,
													 ACX_BA_SESSION_RESPONDER_POLICY,
													 uTid,
													 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].uPolicy,
													 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].aMacAddress,
													 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].uWinSize,
													 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uTid].uInactivityTimeout,
													 NULL,
													 NULL);
					if (tRes != TI_OK)
					{
						return tRes;
					}
				}
			}

			/* Push the last command of the last TID entry into queue with a call back function */
			if (DB_BSS(hCmdBld).bBaInitiator[uLastTid] && !(DB_BSS(hCmdBld).bBaResponder[uLastTid]))
			{

				tRes = cmdBld_CfgIeSetBaSession (hCmdBld,
												 ACX_BA_SESSION_INITIATOR_POLICY,
												 uLastTid,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].uPolicy,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].aMacAddress,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].uWinSize,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].uInactivityTimeout,
												 (void *)cmdBld_ConfigSeq,
												 hCmdBld);
			}
			else if (!(DB_BSS(hCmdBld).bBaInitiator[uLastTid]) && DB_BSS(hCmdBld).bBaResponder[uLastTid])
			{
				tRes = cmdBld_CfgIeSetBaSession (hCmdBld,
												 ACX_BA_SESSION_RESPONDER_POLICY,
												 uLastTid,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].uPolicy,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].aMacAddress,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].uWinSize,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].uInactivityTimeout,
												 (void *)cmdBld_ConfigSeq,
												 hCmdBld);
			}
			else
			{
				/* Initiator & Responsder policy is to be set */
				tRes = cmdBld_CfgIeSetBaSession (hCmdBld,
												 ACX_BA_SESSION_INITIATOR_POLICY,
												 uLastTid,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].uPolicy,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].aMacAddress,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].uWinSize,
												 DB_BSS(hCmdBld).tBaSessionInitiatorPolicy[uLastTid].uInactivityTimeout,
												 NULL,
												 NULL);
				if (tRes != TI_OK)
				{
					return tRes;
				}

				tRes = cmdBld_CfgIeSetBaSession (hCmdBld,
												 ACX_BA_SESSION_RESPONDER_POLICY,
												 uLastTid,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].uPolicy,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].aMacAddress,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].uWinSize,
												 DB_BSS(hCmdBld).tBaSessionResponderPolicy[uLastTid].uInactivityTimeout,
												 (void *)cmdBld_ConfigSeq,
												 hCmdBld);
			}
		}
	}

	return tRes;
}


static TI_STATUS __cfg_tx_power_join (TI_HANDLE hCmdBld)
{
    if (DB_WLAN(hCmdBld).bJoin)
    {
        /* Tx-power must be set after doing join */
        return cmdBld_CfgTxPowerDbm (hCmdBld, DB_WLAN(hCmdBld).TxPowerDbm, (void *)cmdBld_ConfigSeq, hCmdBld);              
    }

    return TI_NOK;
}


static TI_STATUS __cfg_keys (TI_HANDLE hCmdBld)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32  index;

    if (!DB_WLAN(hCmdBld).bJoin)
    {
        return TI_NOK;
    }

    if (pCmdBld->tSecurity.eSecurityMode != TWD_CIPHER_NONE)
    {
        /* 
         * We are doing recovery during security so increase security-sequence-number by 255 just to ensure 
         *   the AP will see progress from last Tx before the recovery (actually needed only for TKIP and AES).
         * Decrementing the low byte by one is handled like it wrpped around, i.e. increment total number by 255.
         */
        cmdBld_SetSecuritySeqNum (hCmdBld, (TI_UINT8)(pCmdBld->uSecuritySeqNumLow - 1));


        /* set the keys to the HW*/
        for (index = 0; 
             index < pCmdBld->tSecurity.uNumOfStations * NO_OF_RECONF_SECUR_KEYS_PER_STATION + NO_OF_EXTRA_RECONF_SECUR_KEYS; 
             index++)
        {
            if ((DB_KEYS(pCmdBld).pReconfKeys + index)->keyType != KEY_NULL)
            {
                if (cmdBld_CmdAddKey (hCmdBld, DB_KEYS(pCmdBld).pReconfKeys + index, TI_TRUE, NULL, NULL) != TI_OK)
                {
                    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "__cfg_keys: ERROR cmdBld_CmdAddKey failure index=%d\n", index);
                    return TI_NOK;
                }   
            }
        }
    
        if (DB_KEYS(pCmdBld).bDefaultKeyIdValid)
        {
            /* Set the deafult key ID to the HW*/
            if (cmdBld_CmdSetWepDefaultKeyId (hCmdBld, DB_KEYS(pCmdBld).uReconfDefaultKeyId, NULL, NULL) != TI_OK)
            {
                TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "__cfg_keys: ERROR cmdBld_CmdSetWepDefaultKeyId failure\n");
                return TI_NOK;
            }   
        }
    }

    /* Set the encryption/decryption control on the HW */   
    if (cmdBld_CfgHwEncDecEnable (hCmdBld, 
                                  DB_KEYS(pCmdBld).bReconfHwEncEnable, 
                                  (void *)cmdBld_ConfigSeq, 
                                  hCmdBld) != TI_OK)
    {
        TRACE0(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "__cfg_keys: ERROR cmdBld_CfgHwEncDecEnable failure \n");
        return TI_NOK;
    }   
    
    return TI_OK;
}

static TI_STATUS __cfg_sg_enable (TI_HANDLE hCmdBld)
{    
    /* Set the Soft Gemini state */
    return cmdBld_CfgSgEnable (hCmdBld, 
                               DB_WLAN(hCmdBld).SoftGeminiEnable,
                               (void *)cmdBld_ConfigSeq, 
                               hCmdBld);
}


static TI_STATUS __cfg_sg (TI_HANDLE hCmdBld)
{    
    /* Set the Soft Gemini params */

	/* signals the FW to config all the paramters from the DB*/
	DB_WLAN(hCmdBld).SoftGeminiParams.paramIdx = 0xFF;

    return cmdBld_CfgSg (hCmdBld, 
                         &DB_WLAN(hCmdBld).SoftGeminiParams, 
                         (void *)cmdBld_ConfigSeq, 
                         hCmdBld);
}


static TI_STATUS __cfg_fm_coex (TI_HANDLE hCmdBld)
{
    /* Set the FM Coexistence params */
    return cmdBld_CfgIeFmCoex (hCmdBld,
                               &DB_WLAN(hCmdBld).tFmCoexParams,
                               (void *)cmdBld_ConfigSeq,
                               hCmdBld);
}


static TI_STATUS __cfg_rate_management (TI_HANDLE hCmdBld)
{
	DB_RM(hCmdBld).rateMngParams.paramIndex = (rateAdaptParam_e) 0xFF;

	return cmdBld_CfgIeRateMngDbg(hCmdBld,
						   &DB_RM(hCmdBld).rateMngParams,
						   (void *)cmdBld_ConfigSeq,
						   hCmdBld);

}


TI_STATUS __itr_memory_map (TI_HANDLE hCmdBld)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    WLAN_OS_REPORT(("Interrogate TX/RX parameters\n"));

    /* Interrogate TX/RX parameters */
    return cmdBld_ItrIeMemoryMap (hCmdBld, 
                                  &pCmdBld->tMemMap, 
                                  (void *)cmdBld_ConfigFwCb, 
                                  hCmdBld);
}


static const TCmdCfgFunc aCmdIniSeq [] =
{
    __cfg_platform_params,
    __cfg_radio_params,
	__cfg_extended_radio_params,
    __cmd_probe_req,
    __cmd_null_data,
    __cmd_disconn,
    __cmd_ps_poll,
    __cmd_qos_null_data,
    __cmd_probe_resp,
    __cmd_beacon,
    __cmd_keep_alive_tmpl,
    __cfg_mem,
    __cfg_rx_msdu_life_time,
    __cfg_rx,
    __cfg_ac_params_0,
    __cfg_tid_0,
    __cfg_ac_params_1,
    __cfg_tid_1,
    __cfg_ac_params_2,
    __cfg_tid_2,
    __cfg_ac_params_3,
    __cfg_tid_3,
    __cfg_pd_threshold,
    __cfg_slot_time,
    __cmd_arp_rsp,
    __cfg_arp_ip_filter,
    __cfg_group_address_table,
    __cfg_service_period_timeout,
    __cfg_rts_threshold,
    __cfg_dco_itrim_params,
    __cfg_fragment_threshold,
    __cfg_pm_config,
    __cfg_beacon_filter_opt,
    __cfg_beacon_filter_table,
    __cfg_tx_cmplt_pacing,
    __cfg_rx_intr_pacing,
    __cfg_sg,
    __cfg_sg_enable,
    __cfg_fm_coex,
    __cfg_cca_threshold,
    __cfg_bcn_brc_options,
    __cmd_enable_rx,
    __cmd_enable_tx,
    __cfg_ps_wmm,
    __cfg_event_scan_cmplt,
    __cfg_event_sps_scan_cmplt,
    __cfg_event_plt_rx_calibration_cmplt,
    __cfg_hw_enc_dec_enable,
    __cfg_rssi_snr_weights,
    __cfg_rssi_snr_trigger_0,
    __cfg_rssi_snr_trigger_1,
    __cfg_rssi_snr_trigger_2,
    __cfg_rssi_snr_trigger_3,
    __cfg_rssi_snr_trigger_4,
    __cfg_rssi_snr_trigger_5,
    __cfg_rssi_snr_trigger_6,
    __cfg_rssi_snr_trigger_7,
    __cfg_max_tx_retry,
    __cfg_split_scan_timeout,

    /* Re-join sequence */ 
    __cfg_tx_rate_policy,
    __cmd_beacon_join,
    __cmd_probe_resp_join,
    __cmd_probe_req_join,
    __cmd_null_data_join,
    __cmd_disconn_join,
    __cmd_ps_poll_join,
    __cmd_keep_alive_tmpl_join,
    __cfg_slot_time_join,
    __cfg_preamble_join,
    __cfg_ht_capabilities,
    __cfg_ht_information,
    __cmd_start_join,
    __cfg_aid,
    __cfg_ba_set_session,
    __cfg_tx_power_join,
    __cfg_keys,
    __cmd_keep_alive_params,
    __cfg_conn_monit_params,
    __cfg_bet,
    __cfg_cts_protection,
    __cfg_ps_rx_streaming,
    __cfg_rx_data_filter,
    __cmd_sta_state,
    __cmd_power_auth,
	__cmd_burst_mode_enable,
	__cfg_rate_management,
    __cmd_arp_rsp_join,
    /* Interrogate command -> must be last!! */
    __itr_memory_map,

    NULL
};


/****************************************************************************
 *                      cmdBld_ConfigSeq()
 ****************************************************************************
 * DESCRIPTION: Configuration sequence engine
 * 
 * INPUTS: None 
 * 
 * OUTPUT: None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_ConfigSeq (TI_HANDLE hCmdBld)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;

    do 
    {
        if (aCmdIniSeq [pCmdBld->uIniSeq++] == NULL)
        {
            return TI_NOK; 
        }
    } 
    while ((*aCmdIniSeq [pCmdBld->uIniSeq - 1])(hCmdBld) != TI_OK);

    return TI_OK;
}

/****************************************************************************
 *                      cmdBld_FinalizeDownload()
 ****************************************************************************
 * DESCRIPTION: Finalize all the remaining initialization after the download has finished 
 * 
 * INPUTS:  
 * 
 * OUTPUT:  None
 * 
 * RETURNS: void
 ****************************************************************************/
void cmdBld_FinalizeDownload (TI_HANDLE hCmdBld, TBootAttr *pBootAttr, FwStaticData_t *pFwInfo)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT8    *pMacAddr = pFwInfo->dot11StationID;
    TI_UINT32    i;
    TI_UINT8     uTmp;

    /* Save FW version */
    os_memoryCopy (pCmdBld->hOs, 
                   (void *)DB_HW(hCmdBld).fwVer, 
                   (void *)pFwInfo->FWVersion, 
                   sizeof(DB_HW(hCmdBld).fwVer));

    /* Save MAC adress (correct the bytes order first) */
    for (i = 0; i < 3; i++)
    {
        uTmp = pMacAddr[i];
        pMacAddr[i] = pMacAddr[5 - i]; 
        pMacAddr[5 - i] = uTmp; 
    }
    MAC_COPY (DB_HW(hCmdBld).macAddress, pMacAddr);

    /* Save chip ID */
    os_memoryCopy (pCmdBld->hOs, 
                   (void *)&(DB_HW(hCmdBld).uHardWareVersion), 
                   (void *)&(pFwInfo->HardWareVersion), 
                   sizeof(DB_HW(hCmdBld).uHardWareVersion));

    /* Save power-levels table */
    os_memoryCopy (pCmdBld->hOs, 
                   (void *)DB_HW(hCmdBld).txPowerTable, 
                   (void *)pFwInfo->txPowerTable, 
                   sizeof(DB_HW(hCmdBld).txPowerTable));

    /* Call the upper layer callback */
    (*((TFinalizeCb)pCmdBld->fFinalizeDownload)) (pCmdBld->hFinalizeDownload);
}


TI_STATUS cmdBld_GetParam (TI_HANDLE hCmdBld, TTwdParamInfo *pParamInfo)
{
    TCmdBld       *pCmdBld = (TCmdBld *)hCmdBld;
    TWlanParams   *pWlanParams = &DB_WLAN(hCmdBld);

    switch (pParamInfo->paramType)
    {
        case TWD_RTS_THRESHOLD_PARAM_ID:
            pParamInfo->content.halCtrlRtsThreshold = pWlanParams->RtsThreshold;
            break;
        
        case TWD_FRAG_THRESHOLD_PARAM_ID:
            pParamInfo->content.halCtrlFragThreshold = pWlanParams->FragmentThreshold;
            break;

        case TWD_COUNTERS_PARAM_ID:
            /* Constant zero because the ACX last buffer next pointer is always pointed
               to itself, so it's like an endless buffer*/
            pParamInfo->content.halCtrlCounters.RecvNoBuffer = 0;
            pParamInfo->content.halCtrlCounters.FragmentsRecv = 0; /* not supported;*/
            pParamInfo->content.halCtrlCounters.FrameDuplicates = 0;/* not supported*/
            pParamInfo->content.halCtrlCounters.FcsErrors = DB_CNT(hCmdBld).FcsErrCnt;
            pParamInfo->content.halCtrlCounters.RecvError = DB_CNT(hCmdBld).FcsErrCnt;
            break;
        
        case TWD_LISTEN_INTERVAL_PARAM_ID:
            pParamInfo->content.halCtrlListenInterval = pWlanParams->ListenInterval;
            break;
                            
        case TWD_RSN_DEFAULT_KEY_ID_PARAM_ID:
            /* Not implemented */
            return TI_NOK;

        case TWD_RSN_SECURITY_MODE_PARAM_ID:
             pParamInfo->content.rsnEncryptionStatus = pCmdBld->tSecurity.eSecurityMode;
            break;

      case TWD_ACX_STATISTICS_PARAM_ID:
            /* Not implemented */
         #if 0
            {
                acxStatisitcs_t     acxStatisitics;
                pParamInfo->content.acxStatisitics.FWpacketReceived = acxStatisitics.FWpacketReceived;
                /* Not supported */
                pParamInfo->content.acxStatisitics.HALpacketReceived = 0; 
            }
         #endif
            return TI_NOK;

    case TWD_MEDIUM_OCCUPANCY_PARAM_ID:
        if (cmdBld_ItrIeMediumOccupancy (hCmdBld, pParamInfo->content.interogateCmdCBParams) != TI_OK)
            return TI_NOK;
            break;

    case TWD_TSF_DTIM_MIB_PARAM_ID:
        if (cmdBld_ItrIeTfsDtim (hCmdBld, pParamInfo->content.interogateCmdCBParams) != TI_OK)
            return TI_NOK;
        break;

    case TWD_AID_PARAM_ID:
        if (cmdBld_GetCurrentAssociationId (hCmdBld, &pParamInfo->content.halCtrlAid) != TI_OK)
            return TI_NOK;

        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , " AID 2 %d\n", pParamInfo->content.halCtrlAid);
        break;

    case TWD_NOISE_HISTOGRAM_PARAM_ID:
        if (cmdBld_ItrIeNoiseHistogramResults (hCmdBld, pParamInfo->content.interogateCmdCBParams) != TI_OK)
        {
            return TI_NOK;
        }
        break;

    case TWD_CURRENT_CHANNEL_PARAM_ID:
        /* Get current channel number */
        pParamInfo->content.halCtrlCurrentChannel = DB_BSS(hCmdBld).RadioChannel;
        break;

    /* SNR and RSSI belongs to the same MIB, and the relevant CB is passed here*/
    case TWD_RSSI_LEVEL_PARAM_ID:
    case TWD_SNR_RATIO_PARAM_ID:
        /* Retrive the Callback function and read buffer pointer that are in fact stored in the TIWLAN_ADAPTER and then send it to the Command Mailbox */
        cmdBld_ItrRSSI (hCmdBld, 
                        pParamInfo->content.interogateCmdCBParams.fCb, 
                        pParamInfo->content.interogateCmdCBParams.hCb, 
                        pParamInfo->content.interogateCmdCBParams.pCb);
        break;

    case TWD_BCN_BRC_OPTIONS_PARAM_ID:
        pParamInfo->content.BcnBrcOptions.BeaconRxTimeout    = pWlanParams->BcnBrcOptions.BeaconRxTimeout;
        pParamInfo->content.BcnBrcOptions.BroadcastRxTimeout = pWlanParams->BcnBrcOptions.BroadcastRxTimeout;
        pParamInfo->content.BcnBrcOptions.RxBroadcastInPs    = pWlanParams->BcnBrcOptions.RxBroadcastInPs;
        break;

    case TWD_MAX_RX_MSDU_LIFE_TIME_PARAM_ID:
        pParamInfo->content.halCtrlMaxRxMsduLifetime = pWlanParams->MaxRxMsduLifetime;
        break;

    case TWD_TX_RATE_CLASS_PARAM_ID:
        pParamInfo->content.pTxRatePlicy = &DB_BSS(hCmdBld).TxRateClassParams;
        break;

    case TWD_SG_CONFIG_PARAM_ID:
        return cmdBld_ItrSg (hCmdBld,
                             pParamInfo->content.interogateCmdCBParams.fCb, 
                             pParamInfo->content.interogateCmdCBParams.hCb,
                             (void*)pParamInfo->content.interogateCmdCBParams.pCb);

    case TWD_TX_POWER_PARAM_ID:
        pParamInfo->content.halCtrlTxPowerDbm = DB_WLAN(hCmdBld).TxPowerDbm;        
        break;

    case TWD_RADIO_TEST_PARAM_ID:
        TRACE0(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION , "Radio Test\n");
        return cmdBld_CmdTest (hCmdBld,
                               pParamInfo->content.interogateCmdCBParams.fCb,
                               pParamInfo->content.interogateCmdCBParams.hCb,
                               (TTestCmd*)pParamInfo->content.interogateCmdCBParams.pCb);

    case TWD_DCO_ITRIM_PARAMS_ID:
        pParamInfo->content.tDcoItrimParams.enable = pWlanParams->dcoItrimEnabled;
        pParamInfo->content.tDcoItrimParams.moderationTimeoutUsec = pWlanParams->dcoItrimModerationTimeoutUsec;
        break;

    default:
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_GetParam - ERROR - Param is not supported, %d\n\n", pParamInfo->paramType);
        return (PARAM_NOT_SUPPORTED);
    }

    return TI_OK;
}


static TI_STATUS cmdBld_ReadMibBeaconFilterIETable (TI_HANDLE hCmdBld, TI_HANDLE hCb, void* fCb, void* pCb)
{
    TCmdBld *pCmdBld                = (TCmdBld *)hCmdBld;
    TMib *pMib                      = (TMib*)pCb;
    TCmdQueueInterrogateCb RetFunc  = (TCmdQueueInterrogateCb)fCb;
    TI_UINT8 IETableSize            = 0;

    /*Get params*/
    pMib->aData.BeaconFilter.iNumberOfIEs = DB_WLAN(hCmdBld).beaconFilterIETable.numberOfIEs;
    IETableSize = DB_WLAN(hCmdBld).beaconFilterIETable.IETableSize;
                  
    os_memoryZero (pCmdBld->hOs, 
                   pMib->aData.BeaconFilter.iIETable,
                   sizeof(pMib->aData.BeaconFilter.iIETable));

    os_memoryCopy (pCmdBld->hOs, 
                   pMib->aData.BeaconFilter.iIETable,
                   DB_WLAN(hCmdBld).beaconFilterIETable.IETable,
                   IETableSize);

    pMib->Length = IETableSize + 1;     

    RetFunc(hCb, TI_OK, pCb); 

    return TI_OK;
}

/**
 * \author \n
 * \date \n
 * \brief Coordinates between legacy TxRatePolicy implementation and the MIB format: \n
 *        Converts the pGwsi_txRatePolicy back to whal commands 
 *        Activates the whal whalCtrl_set function 
 * Function Scope \e Public.\n
 * \param  - \n
 * \return \n
 */
static TI_STATUS cmdBld_ReadMibTxRatePolicy (TI_HANDLE hCmdBld, TI_HANDLE hCb, void* fCb, void* pCb)
{
    TMib* pMib = (TMib*)pCb;
    TCmdQueueInterrogateCb RetFunc = (TCmdQueueInterrogateCb)fCb;
    TTwdParamInfo param;
    TI_STATUS status = TI_OK;

    param.paramType = TWD_TX_RATE_CLASS_PARAM_ID;
    cmdBld_GetParam (hCmdBld, &param);
    if (param.content.pTxRatePlicy == NULL)
        return TI_NOK;

    /*Copy the data form the param to the MIB*/
    pMib->aData.txRatePolicy = *param.content.pTxRatePlicy;
    pMib->Length = pMib->aData.txRatePolicy.numOfRateClasses * sizeof(pMib->aData.txRatePolicy.rateClass[0]) + 
                   sizeof(pMib->aData.txRatePolicy.numOfRateClasses);
    RetFunc (hCb, status, pCb);
    return status;
}


TI_STATUS cmdBld_ReadMib (TI_HANDLE hCmdBld, TI_HANDLE hCb, void* fCb, void* pCb)
{
    TCmdBld     *pCmdBld = (TCmdBld *)hCmdBld;
    TMib    *pMibBuf = (TMib*)pCb;
    TCmdQueueInterrogateCb RetFunc = (TCmdQueueInterrogateCb)fCb;
    TI_STATUS Status = TI_OK;
    
    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_ReadMib :pMibBuf %p:\n",pMibBuf);
    
    TRACE1(pCmdBld->hReport, REPORT_SEVERITY_INFORMATION, "cmdBld_ReadMib :aMib %x:\n", pMibBuf->aMib);
    
    switch (pMibBuf->aMib)
    {
    case MIB_dot11MaxReceiveLifetime:
        {
            TTwdParamInfo ParamInfo;
            ParamInfo.paramType = TWD_MAX_RX_MSDU_LIFE_TIME_PARAM_ID;
            ParamInfo.paramLength = sizeof(ParamInfo.content.halCtrlMaxRxMsduLifetime);
            Status = cmdBld_GetParam (hCmdBld, &ParamInfo);
            pMibBuf->aData.MaxReceiveLifeTime = ParamInfo.content.halCtrlMaxRxMsduLifetime / 1024; /* converting from usecs to TUs*/
            pMibBuf->Length = sizeof(pMibBuf->aData.MaxReceiveLifeTime);
        }
        break;
        
    case MIB_dot11GroupAddressesTable:
        {
            Status = cmdBld_GetGroupAddressTable (hCmdBld,
                                                  &pMibBuf->aData.GroupAddressTable.bFilteringEnable,
                                                  &pMibBuf->aData.GroupAddressTable.nNumberOfAddresses,
                                                  pMibBuf->aData.GroupAddressTable.aGroupTable);
            
            pMibBuf->Length = sizeof(pMibBuf->aData.GroupAddressTable.bFilteringEnable) + 
                              sizeof(pMibBuf->aData.GroupAddressTable.nNumberOfAddresses) +
                              pMibBuf->aData.GroupAddressTable.nNumberOfAddresses * sizeof(TMacAddr);
        }
        break;
        
    case MIB_ctsToSelf:
        {
            TTwdParamInfo ParamInfo;
            ParamInfo.paramType = TWD_CTS_TO_SELF_PARAM_ID;
            ParamInfo.paramLength = sizeof(ParamInfo.content.halCtrlCtsToSelf);
            Status = cmdBld_GetParam (hCmdBld, &ParamInfo);
            pMibBuf->aData.CTSToSelfEnable = ParamInfo.content.halCtrlCtsToSelf;
            pMibBuf->Length = sizeof(pMibBuf->aData.CTSToSelfEnable);
        }
        break;
        
    case MIB_arpIpAddressesTable:
        {
            TIpAddr   IpAddress;  
            EIpVer    IPver;
            TI_UINT8  Enable;
            
            pMibBuf->Length = sizeof(TMibArpIpAddressesTable);
            Status = cmdBld_GetArpIpAddressesTable (hCmdBld, &IpAddress, &Enable, &IPver);
            if (Status == TI_OK)
            {
                pMibBuf->aData.ArpIpAddressesTable.FilteringEnable = Enable;         

                if (IP_VER_4 == IPver) /* IP_VER_4 only */
                {
                    IP_COPY (pMibBuf->aData.ArpIpAddressesTable.addr, IpAddress);
                }
                else
                {
                    Status = TI_NOK;
                }
            }
            return Status;
        }
        
    case MIB_rxFilter:
        {
            TI_UINT32 RxConfigOption;
            TI_UINT32 RxFilterOption;
            
            pMibBuf->Length = 1;
            pMibBuf->aData.RxFilter = 0;

            /* Get RX filter data */
            Status = cmdBld_GetRxFilters (hCmdBld, &RxConfigOption, &RxFilterOption);
            if (TI_OK == Status)
            {
                /*Translate to MIB bitmap*/
                if ((RxConfigOption & RX_CFG_MAC) == RX_CFG_ENABLE_ANY_DEST_MAC)
                    pMibBuf->aData.RxFilter |= MIB_RX_FILTER_PROMISCOUS_SET;
                
                if ((RxConfigOption & RX_CFG_BSSID) == RX_CFG_ENABLE_ONLY_MY_BSSID)
                    pMibBuf->aData.RxFilter |= MIB_RX_FILTER_BSSID_SET;
            }
        }
        break;

    case MIB_beaconFilterIETable:
        return cmdBld_ReadMibBeaconFilterIETable (hCmdBld, hCb, fCb, pCb);

    case MIB_txRatePolicy:
        return cmdBld_ReadMibTxRatePolicy (hCmdBld, hCb, fCb, pCb);

    case MIB_countersTable:
        return cmdBld_ItrErrorCnt (hCmdBld, fCb, hCb, pCb);

    case MIB_statisticsTable:
        return cmdBld_ItrRoamimgStatisitics (hCmdBld, fCb, hCb, pCb);

    default:
        TRACE1(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "TWD_ReadMib:MIB aMib 0x%x Not supported\n",pMibBuf->aMib);
        return TI_NOK;
    }

    if(RetFunc)
        RetFunc(hCb, Status, pCb);
    
    return TI_OK;
}


TI_STATUS cmdBld_GetGroupAddressTable (TI_HANDLE hCmdBld, TI_UINT8* pEnabled, TI_UINT8* pNumGroupAddrs, TMacAddr *pGroupAddr)
{
    TCmdBld   *pCmdBld = (TCmdBld *)hCmdBld;
    TI_UINT32     i;

    if (NULL == pEnabled || NULL == pNumGroupAddrs || NULL == pGroupAddr)
    {
        TRACE3(pCmdBld->hReport, REPORT_SEVERITY_ERROR, "cmdBld_GetGroupAddressTable: pisEnabled=0x%p pnumGroupAddrs=0x%p  Group_addr=0x%p !!!\n", pEnabled, pNumGroupAddrs, pGroupAddr);
        return PARAM_VALUE_NOT_VALID;
    }

    *pNumGroupAddrs = DB_WLAN(hCmdBld).numGroupAddrs;
    *pEnabled = DB_WLAN(hCmdBld).isMacAddrFilteringnabled;

    os_memoryZero (pCmdBld->hOs, pGroupAddr, sizeof(pGroupAddr));
    for (i = 0; i < *pNumGroupAddrs; i++) 
    {
        os_memoryCopy (pCmdBld->hOs, 
                       (void *)&((*pGroupAddr)[MAC_ADDR_LEN*i]), 
                       &DB_WLAN(hCmdBld).aGroupAddr[i], 
                       MAC_ADDR_LEN);
    }

    return TI_OK;
}


TI_STATUS cmdBld_GetRxFilters (TI_HANDLE hCmdBld, TI_UINT32* pRxConfigOption, TI_UINT32* pRxFilterOption)
{
    *pRxConfigOption = DB_WLAN(hCmdBld).RxConfigOption;
    *pRxFilterOption = DB_WLAN(hCmdBld).RxFilterOption;

    return TI_OK;
}


TFwInfo * cmdBld_GetFWInfo (TI_HANDLE hCmdBld)
{
    return &DB_HW(hCmdBld);
}



TI_STATUS cmdBld_SetRadioBand (TI_HANDLE hCmdBld, ERadioBand eRadioBand)
{
    DB_WLAN(hCmdBld).RadioBand = eRadioBand;       
    
    return TI_OK;
}


/****************************************************************************
 *                      cmdBld_CurrentAssociationIdGet()
 ****************************************************************************
 * DESCRIPTION: Get the current TX antenna 
 * 
 * INPUTS:  
 * 
 * OUTPUT:  
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_GetCurrentAssociationId (TI_HANDLE hCmdBld, TI_UINT16 *pAidVal)
{
    *pAidVal = DB_WLAN(hCmdBld).Aid;

    return TI_OK;
}


 /****************************************************************************
 *                      cmdBld_GetArpIpAddressesTable()
 ****************************************************************************
 * DESCRIPTION: Sets the Group table according to the given configuration. 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
TI_STATUS cmdBld_GetArpIpAddressesTable (TI_HANDLE hCmdBld, TIpAddr *pIp, TI_UINT8* pbEnabled, EIpVer *pIpVer)
{
    *pIpVer = (EIpVer)DB_WLAN(hCmdBld).arp_IP_ver;

    IP_COPY (*pIp, DB_WLAN(hCmdBld).arp_IP_addr);

    *pbEnabled = (TI_UINT8)DB_WLAN(hCmdBld).arpFilterType;

    return TI_OK;
}


TI_STATUS cmdBld_ConvertAppRatesBitmap (TI_UINT32 uAppRatesBitmap, TI_UINT32 uAppModulation, EHwRateBitFiled *pHwRatesBitmap)
{
    EHwRateBitFiled uRatesBitmap = 0;
   
    if (uAppRatesBitmap & DRV_RATE_MASK_1_BARKER)    uRatesBitmap |= HW_BIT_RATE_1MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_2_BARKER)    uRatesBitmap |= HW_BIT_RATE_2MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_5_5_CCK)     uRatesBitmap |= HW_BIT_RATE_5_5MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_11_CCK)      uRatesBitmap |= HW_BIT_RATE_11MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_22_PBCC)     uRatesBitmap |= HW_BIT_RATE_22MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_6_OFDM)      uRatesBitmap |= HW_BIT_RATE_6MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_9_OFDM)      uRatesBitmap |= HW_BIT_RATE_9MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_12_OFDM)     uRatesBitmap |= HW_BIT_RATE_12MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_18_OFDM)     uRatesBitmap |= HW_BIT_RATE_18MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_24_OFDM)     uRatesBitmap |= HW_BIT_RATE_24MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_36_OFDM)     uRatesBitmap |= HW_BIT_RATE_36MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_48_OFDM)     uRatesBitmap |= HW_BIT_RATE_48MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_54_OFDM)     uRatesBitmap |= HW_BIT_RATE_54MBPS;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_0_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_0;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_1_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_1;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_2_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_2;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_3_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_3;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_4_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_4;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_5_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_5;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_6_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_6;
    if (uAppRatesBitmap & DRV_RATE_MASK_MCS_7_OFDM)  uRatesBitmap |= HW_BIT_RATE_MCS_7;
    
    *pHwRatesBitmap = uRatesBitmap;

    return TI_OK;
}

EHwRateBitFiled rateNumberToBitmap(TI_UINT8 uRate)
{
	switch(uRate)
	{
	case 1:   return HW_BIT_RATE_1MBPS;
	case 2:   return HW_BIT_RATE_2MBPS;
	case 5:   return HW_BIT_RATE_5_5MBPS;
	case 6:   return HW_BIT_RATE_6MBPS; 
	case 9:   return HW_BIT_RATE_9MBPS; 
	case 11:  return HW_BIT_RATE_11MBPS;
	case 12:  return HW_BIT_RATE_12MBPS;
	case 18:  return HW_BIT_RATE_18MBPS;
	case 22:  return HW_BIT_RATE_22MBPS;
	case 24:  return HW_BIT_RATE_24MBPS;
	case 36:  return HW_BIT_RATE_36MBPS;
	case 48:  return HW_BIT_RATE_48MBPS;
	case 54:  return HW_BIT_RATE_54MBPS;
	default:
		return 0;
	}
}

TI_STATUS cmdBld_ConvertAppRate (ERate AppRate, TI_UINT8 *pHwRate)
{
    TI_UINT8     Rate = 0;
    TI_STATUS status = TI_OK;

    switch (AppRate)
    {
        /*
         *  The handle for 5.5/11/22 PBCC was removed !!!
         */

        case DRV_RATE_1M:           Rate = txPolicy1;          break;
        case DRV_RATE_2M:           Rate = txPolicy2;          break;
        case DRV_RATE_5_5M:         Rate = txPolicy5_5;        break;
        case DRV_RATE_11M:          Rate = txPolicy11;         break;
        case DRV_RATE_22M:          Rate = txPolicy22;         break;
        case DRV_RATE_6M:           Rate = txPolicy6;          break;
        case DRV_RATE_9M:           Rate = txPolicy9;          break;
        case DRV_RATE_12M:          Rate = txPolicy12;         break;
        case DRV_RATE_18M:          Rate = txPolicy18;         break;
        case DRV_RATE_24M:          Rate = txPolicy24;         break;
        case DRV_RATE_36M:          Rate = txPolicy36;         break;
        case DRV_RATE_48M:          Rate = txPolicy48;         break;
        case DRV_RATE_54M:          Rate = txPolicy54;         break;
        case DRV_RATE_MCS_0:          Rate = txPolicyMcs0;         break;
        case DRV_RATE_MCS_1:          Rate = txPolicyMcs1;         break;
        case DRV_RATE_MCS_2:          Rate = txPolicyMcs2;         break;
        case DRV_RATE_MCS_3:          Rate = txPolicyMcs3;         break;
        case DRV_RATE_MCS_4:          Rate = txPolicyMcs4;         break;
        case DRV_RATE_MCS_5:          Rate = txPolicyMcs5;         break;
        case DRV_RATE_MCS_6:          Rate = txPolicyMcs6;         break;
        case DRV_RATE_MCS_7:          Rate = txPolicyMcs7;         break;

        default:
            WLAN_OS_REPORT(("%s wrong app rate = %d\n",__FUNCTION__,AppRate));
            status = TI_NOK;
            break;
    }

    if (status == TI_OK)
        *pHwRate = Rate;
    else
        *pHwRate = txPolicy1; 

    return status;
}


TI_STATUS cmdBld_SetRxFilter (TI_HANDLE hCmdBld, TI_UINT32 uRxConfigOption, TI_UINT32 uRxFilterOption)
{
    DB_WLAN(hCmdBld).RxConfigOption = uRxConfigOption;
    DB_WLAN(hCmdBld).RxFilterOption = uRxFilterOption;
    DB_WLAN(hCmdBld).RxConfigOption |= RX_CFG_ENABLE_PHY_HEADER_PLCP;

    if (DB_WLAN(hCmdBld).RxDisableBroadcast)
    {
        DB_WLAN(hCmdBld).RxConfigOption |= RX_CFG_DISABLE_BCAST;
    }
    
    return TI_OK;
}


TI_UINT8 cmdBld_GetBssType (TI_HANDLE hCmdBld)
{
    return DB_BSS(hCmdBld).ReqBssType;
}


TI_UINT32 cmdBld_GetAckPolicy (TI_HANDLE hCmdBld, TI_UINT32 uQueueId)
{
    return (TI_UINT32)DB_QUEUES(hCmdBld).queues[uQueueId].ackPolicy;
}


TI_STATUS cmdBld_SetSecuritySeqNum (TI_HANDLE hCmdBld, TI_UINT8 securitySeqNumLsByte)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    /* If 8 lsb wrap around occurred (new < old). */
    if ((TI_UINT16)securitySeqNumLsByte < (pCmdBld->uSecuritySeqNumLow & 0xFF))
    {
        /* Increment the upper byte of the 16 lsb. */       
        pCmdBld->uSecuritySeqNumLow += 0x100;

        /* If 16 bit wrap around occurred, increment the upper 32 bit. */
        if (!(pCmdBld->uSecuritySeqNumLow & 0xFF00))
            pCmdBld->uSecuritySeqNumHigh++;
    }

    /* Save new sequence number 8 lsb (received from the FW). */
    pCmdBld->uSecuritySeqNumLow &= 0xFF00;
    pCmdBld->uSecuritySeqNumLow |= (TI_UINT16)securitySeqNumLsByte;

    return TI_OK;
}

/****************************************************************************
 *                      cmdBld_JoinCmpltForReconfigCb()
 ****************************************************************************
 * DESCRIPTION:   The Join-Complete callback used by the reconfig sequenc (see __cmd_start_join()).
 *                It restores the original Join-Complete CB and continues the sequence.
 *                It is needed so the reconfig sequence won't progress before the Join
 *                    command is completed (otherwise the FW may drop the other commands).
 *
 * INPUTS: hCmdBld - The module object 
 * 
 * OUTPUT:  None
 * 
 * RETURNS: TI_OK
 ****************************************************************************/
static TI_STATUS cmdBld_JoinCmpltForReconfigCb (TI_HANDLE hCmdBld)
{
    TCmdBld    *pCmdBld = (TCmdBld *)hCmdBld;
    void       *fDummyCb;    
    TI_HANDLE   hDummyHndl;  

    /* Restored the original Join-Complete callback function */
    eventMbox_ReplaceEvent (pCmdBld->hEventMbox, 
                              TWD_OWN_EVENT_JOIN_CMPLT, 
                              pCmdBld->fJoinCmpltOriginalCbFunc, 
                              pCmdBld->hJoinCmpltOriginalCbHndl,
                              &fDummyCb,
                              &hDummyHndl);  

    /* Call the reconfig sequence to continue the configuration after Join completion */
    cmdBld_ConfigSeq (hCmdBld);

    return TI_OK;
}



static TI_STATUS cmdBld_DummyCb (TI_HANDLE hCmdBld)
{
    return TI_OK;
}





#ifdef TI_DBG

void cmdBld_DbgForceTemplatesRates (TI_HANDLE hCmdBld, TI_UINT32 uRateMask)
{
    TCmdBld *pCmdBld = (TCmdBld *)hCmdBld;

    pCmdBld->uDbgTemplatesRateMask = uRateMask; 
}

#endif /* TI_DBG */

