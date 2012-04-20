/*
 * rx.c
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

/***************************************************************************/
/*                                                                         */
/*      MODULE: Rx.c                                                       */
/*    PURPOSE:  Rx module functions                                        */
/*                                                                         */
/***************************************************************************/
#define __FILE_ID__  FILE_ID_54
#include "tidef.h"
#include "paramOut.h"
#include "rx.h"
#include "osApi.h"
#include "timer.h"
#include "DataCtrl_Api.h"
#include "Ctrl.h"
#include "802_11Defs.h"
#include "Ethernet.h"
#include "report.h"
#include "rate.h"
#include "mlmeApi.h"
#include "rsnApi.h"
#include "smeApi.h"
#include "siteMgrApi.h"
#include "GeneralUtil.h"
#include "EvHandler.h"
#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif
#include "TWDriver.h"
#include "RxBuf.h"
#include "DrvMainModules.h"
#include "bmtrace_api.h"
#include "PowerMgr_API.h"


#define EAPOL_PACKET                    0x888E
#define IAPP_PACKET                     0x0000
#define PREAUTH_EAPOL_PACKET            0x88C7


#define RX_DATA_FILTER_FLAG_NO_BIT_MASK         0
#define RX_DATA_FILTER_FLAG_USE_BIT_MASK        1
#define RX_DATA_FILTER_FLAG_IP_HEADER           0
#define RX_DATA_FILTER_FLAG_ETHERNET_HEADER     2
#define RX_DATA_FILTER_ETHERNET_HEADER_BOUNDARY 14

#define PADDING_ETH_PACKET_SIZE                 2

/* CallBack for recieving packet from rxXfer */
static void rxData_ReceivePacket (TI_HANDLE   hRxData,  void  *pBuffer);

static ERxBufferStatus rxData_RequestForBuffer (TI_HANDLE   hRxData, void **pBuf, TI_UINT16 aLength, TI_UINT32 uEncryptionFlag,PacketClassTag_e ePacketClassTag);

#if 0
static TI_STATUS rxData_checkBssIdAndBssType(TI_HANDLE hRxData, 
											 dot11_header_t* dot11_header,
											 TMacAddr **rxBssid, 
											 ScanBssType_e  *currBssType,
											 TMacAddr  *currBssId);
#endif
static TI_STATUS rxData_convertWlanToEthHeader (TI_HANDLE hRxData, void *pBuffer, TI_UINT16 * etherType);
static TI_STATUS rxData_ConvertAmsduToEthPackets (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);
static void rxData_dataPacketDisptcher (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);
static void rxData_discardPacket (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);
static void rxData_discardPacketVlan (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);
static void rxData_rcvPacketInOpenNotify (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);
static void rxData_rcvPacketEapol (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);
static void rxData_rcvPacketData (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);

static TI_STATUS rxData_enableDisableRxDataFilters(TI_HANDLE hRxData, TI_BOOL enabled);
static TI_STATUS rxData_addRxDataFilter(TI_HANDLE hRxData, TRxDataFilterRequest* request);
static TI_STATUS rxData_removeRxDataFilter(TI_HANDLE hRxData, TRxDataFilterRequest* request);


#ifdef XCC_MODULE_INCLUDED
static void rxData_rcvPacketIapp(TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr);
#endif
#ifdef TI_DBG
static void rxData_printRxThroughput(TI_HANDLE hRxData, TI_BOOL bTwdInitOccured);
#endif

static void rxData_StartReAuthActiveTimer(TI_HANDLE hRxData);
static void reAuthTimeout(TI_HANDLE hRxData, TI_BOOL bTwdInitOccured);
static void rxData_ReauthEnablePriority(TI_HANDLE hRxData);


/*************************************************************************
*                        rxData_create                                   *
**************************************************************************
* DESCRIPTION:  This function initializes the Rx data module.                 
*                                                      
* INPUT:        hOs - handle to Os Abstraction Layer
*
* OUTPUT:      
*
* RETURN:       Handle to the allocated Rx data control block
************************************************************************/
TI_HANDLE rxData_create (TI_HANDLE hOs)
{
    rxData_t *pRxData;

    /* check parameters validity */
    if (hOs == NULL)
    {
        WLAN_OS_REPORT(("FATAL ERROR: rxData_create(): OS handle Error - Aborting\n"));
        return NULL;
    }
    

    /* alocate Rx module control block */
    pRxData = os_memoryAlloc(hOs, (sizeof(rxData_t)));

    if (!pRxData)
    {
        WLAN_OS_REPORT(("FATAL ERROR: rxData_create(): Error Creating Rx Module - Aborting\n"));
        return NULL;
    }

    /* reset Rx control block */
    os_memoryZero (hOs, pRxData, (sizeof(rxData_t)));

    pRxData->RxEventDistributor = DistributorMgr_Create (hOs, MAX_RX_NOTIF_REQ_ELMENTS);

    pRxData->hOs = hOs;

    return (TI_HANDLE)pRxData;
}

/***************************************************************************
*                           rxData_config                                  *
****************************************************************************
* DESCRIPTION:  This function configures the Rx Data module     
* 
* INPUTS:       pStadHandles  - The driver modules handles
*
* OUTPUT:       
* 
* RETURNS:      void
***************************************************************************/
void rxData_init (TStadHandlesList *pStadHandles)
{
    rxData_t *pRxData = (rxData_t *)(pStadHandles->hRxData);

    pRxData->hCtrlData  = pStadHandles->hCtrlData; 
    pRxData->hTWD       = pStadHandles->hTWD;
    pRxData->hMlme      = pStadHandles->hMlmeSm; 
    pRxData->hRsn       = pStadHandles->hRsn;
    pRxData->hSiteMgr   = pStadHandles->hSiteMgr;
    pRxData->hOs        = pStadHandles->hOs;
    pRxData->hReport    = pStadHandles->hReport;
    pRxData->hXCCMgr    = pStadHandles->hXCCMngr;
    pRxData->hEvHandler = pStadHandles->hEvHandler;
    pRxData->hTimer     = pStadHandles->hTimer;
    pRxData->hPowerMgr  = pStadHandles->hPowerMgr;

    pRxData->rxDataExcludeUnencrypted = DEF_EXCLUDE_UNENCYPTED; 
    pRxData->rxDataExludeBroadcastUnencrypted = DEF_EXCLUDE_UNENCYPTED;
    pRxData->rxDataEapolDestination = DEF_EAPOL_DESTINATION;
    pRxData->rxDataPortStatus = DEF_RX_PORT_STATUS;
	pRxData->genericEthertype = EAPOL_PACKET;

    /*
     * configure rx data dispatcher 
     */

    /* port status close */
    pRxData->rxData_dispatchBuffer[CLOSE][DATA_DATA_PACKET]  = &rxData_discardPacket;       /* data  */
    pRxData->rxData_dispatchBuffer[CLOSE][DATA_EAPOL_PACKET] = &rxData_discardPacket;       /* eapol */
    pRxData->rxData_dispatchBuffer[CLOSE][DATA_IAPP_PACKET]  = &rxData_discardPacket;       /* Iapp  */
    pRxData->rxData_dispatchBuffer[CLOSE][DATA_VLAN_PACKET]  = &rxData_discardPacketVlan;   /* VLAN  */

    /* port status open notify */
    pRxData->rxData_dispatchBuffer[OPEN_NOTIFY][DATA_DATA_PACKET]  = &rxData_rcvPacketInOpenNotify; /* data  */ 
    pRxData->rxData_dispatchBuffer[OPEN_NOTIFY][DATA_EAPOL_PACKET] = &rxData_rcvPacketInOpenNotify; /* eapol */ 
    pRxData->rxData_dispatchBuffer[OPEN_NOTIFY][DATA_IAPP_PACKET]  = &rxData_rcvPacketInOpenNotify; /* Iapp  */ 
    pRxData->rxData_dispatchBuffer[OPEN_NOTIFY][DATA_VLAN_PACKET]  = &rxData_discardPacketVlan;     /* VLAN  */

    /* port status open eapol */
    pRxData->rxData_dispatchBuffer[OPEN_EAPOL][DATA_DATA_PACKET]  = &rxData_discardPacket;      /* data  */ 
    pRxData->rxData_dispatchBuffer[OPEN_EAPOL][DATA_EAPOL_PACKET] = &rxData_rcvPacketEapol;     /* eapol */ 
    pRxData->rxData_dispatchBuffer[OPEN_EAPOL][DATA_IAPP_PACKET]  = &rxData_discardPacket;      /* Iapp  */ 
    pRxData->rxData_dispatchBuffer[OPEN_EAPOL][DATA_VLAN_PACKET]  = &rxData_discardPacketVlan;  /* VLAN  */

    /* port status open */
    pRxData->rxData_dispatchBuffer[OPEN][DATA_DATA_PACKET]  = &rxData_rcvPacketData;    /* data  */ 
    pRxData->rxData_dispatchBuffer[OPEN][DATA_EAPOL_PACKET] = &rxData_rcvPacketEapol;   /* eapol */ 
#ifdef XCC_MODULE_INCLUDED
    pRxData->rxData_dispatchBuffer[OPEN][DATA_IAPP_PACKET]  = &rxData_rcvPacketIapp;    /* Iapp  */ 
#else
    pRxData->rxData_dispatchBuffer[OPEN][DATA_IAPP_PACKET]  = &rxData_rcvPacketData;    /* Iapp  */ 
#endif
    pRxData->rxData_dispatchBuffer[OPEN][DATA_VLAN_PACKET]  = &rxData_discardPacketVlan;/* VLAN  */

    /* register CB's for request buffer and receive CB to the lower layers */
    TWD_RegisterCb (pRxData->hTWD,
                    TWD_EVENT_RX_RECEIVE_PACKET,
                    (void *)rxData_ReceivePacket, 
                    pStadHandles->hRxData);

    TWD_RegisterCb (pRxData->hTWD,
                    TWD_EVENT_RX_REQUEST_FOR_BUFFER,
                    (void*)rxData_RequestForBuffer, 
                    pStadHandles->hRxData);
}


TI_STATUS rxData_SetDefaults (TI_HANDLE hRxData, rxDataInitParams_t * rxDataInitParams)
{
    rxData_t *pRxData = (rxData_t *)hRxData;
    int i;

    /* init rx data filters */
    pRxData->filteringEnabled = rxDataInitParams->rxDataFiltersEnabled;
    pRxData->filteringDefaultAction = rxDataInitParams->rxDataFiltersDefaultAction;
    TWD_CfgEnableRxDataFilter (pRxData->hTWD, pRxData->filteringEnabled, pRxData->filteringDefaultAction);

    for (i = 0; i < MAX_DATA_FILTERS; ++i)
    {
        if (rxDataInitParams->rxDataFilterRequests[i].maskLength > 0)
        {
            if (rxData_addRxDataFilter(hRxData, &rxDataInitParams->rxDataFilterRequests[i]) != TI_OK)
            {
                TRACE1(pRxData->hReport, REPORT_SEVERITY_ERROR, ": Invalid Rx Data Filter configured at init stage (at index %d)!\n", i);
            }
        }
    }

	pRxData->reAuthActiveTimer = tmr_CreateTimer (pRxData->hTimer);
	if (pRxData->reAuthActiveTimer == NULL)
	{
        WLAN_OS_REPORT(("rxData_SetDefaults(): Failed to create reAuthActiveTimer!\n"));
		return TI_NOK;
	}

    pRxData->reAuthActiveTimeout = rxDataInitParams->reAuthActiveTimeout;

	rxData_SetReAuthInProgress(pRxData, TI_FALSE);

#ifdef TI_DBG
    /* reset counters */
    rxData_resetCounters(pRxData);
    rxData_resetDbgCounters(pRxData);

    /* allocate timer for debug throughput */
    pRxData->hThroughputTimer = tmr_CreateTimer (pRxData->hTimer);
    if (pRxData->hThroughputTimer == NULL)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, "rxData_SetDefaults(): Failed to create hThroughputTimer!\n");
        return TI_NOK;
    }
    pRxData->rxThroughputTimerEnable = TI_FALSE;
#endif


    TRACE0(pRxData->hReport, REPORT_SEVERITY_INIT, ".....Rx Data configured successfully\n");

    return TI_OK;
}

/***************************************************************************
*                           rxData_unLoad                                  *
****************************************************************************
* DESCRIPTION:  This function unload the Rx data module. 
* 
* INPUTS:       hRxData - the object
*       
* OUTPUT:       
* 
* RETURNS:      TI_OK - Unload succesfull
*               TI_NOK - Unload unsuccesfull
***************************************************************************/
TI_STATUS rxData_unLoad(TI_HANDLE hRxData)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    /* check parameters validity */
    if (pRxData == NULL)
    {
        return TI_NOK;
    }

    DistributorMgr_Destroy(pRxData->RxEventDistributor);

#ifdef TI_DBG
    /* destroy periodic rx throughput timer */
	if (pRxData->hThroughputTimer)
	{
		tmr_DestroyTimer (pRxData->hThroughputTimer);
	}
  #endif

	if (pRxData->reAuthActiveTimer)
	{
		tmr_DestroyTimer (pRxData->reAuthActiveTimer);
	}

    /* free Rx Data controll block */
    os_memoryFree(pRxData->hOs, pRxData, sizeof(rxData_t));

    return TI_OK;
}


/***************************************************************************
*                           rxData_stop                                    *
****************************************************************************
* DESCRIPTION:  this function stop the rx data. 
* 
* INPUTS:       hRxData - the object
*       
* OUTPUT:       
* 
* RETURNS:      TI_OK - stop succesfull
*               TI_NOK - stop unsuccesfull
***************************************************************************/
TI_STATUS rxData_stop (TI_HANDLE hRxData)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    /* check parameters validity */
    if (pRxData == NULL)
    {
        return TI_NOK;
    }

    pRxData->rxDataExcludeUnencrypted = DEF_EXCLUDE_UNENCYPTED; 
    pRxData->rxDataExludeBroadcastUnencrypted = DEF_EXCLUDE_UNENCYPTED;
    pRxData->rxDataEapolDestination = DEF_EAPOL_DESTINATION;
    pRxData->rxDataPortStatus = DEF_RX_PORT_STATUS;

  #ifdef TI_DBG
    /* reset counters */
    /*rxData_resetCounters(pRxData);*/
    /*rxData_resetDbgCounters(pRxData);*/

    /* stop throughput timer */
    if (pRxData->rxThroughputTimerEnable)
    {
        tmr_StopTimer (pRxData->hThroughputTimer);
        pRxData->rxThroughputTimerEnable = TI_FALSE;
    }
  #endif

    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_stop() :  Succeeded.\n");

    return TI_OK;

}

/***************************************************************************
*                           rxData_getParam                                *
****************************************************************************
* DESCRIPTION:  get a specific parameter
* 
* INPUTS:       hRxData - the object
*               
* OUTPUT:       pParamInfo - structure which include the value of 
*               the requested parameter
* 
* RETURNS:      TI_OK
*               TI_NOK
***************************************************************************/
TI_STATUS rxData_getParam(TI_HANDLE hRxData, paramInfo_t *pParamInfo)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    /* check handle validity */
    if (pRxData == NULL)
    {
        return TI_NOK;
    }

    switch (pParamInfo->paramType)
    {
        case RX_DATA_EXCLUDE_UNENCRYPTED_PARAM:
            pParamInfo->content.rxDataExcludeUnencrypted = pRxData->rxDataExcludeUnencrypted;
            break;

        case RX_DATA_EAPOL_DESTINATION_PARAM:
            pParamInfo->content.rxDataEapolDestination = pRxData->rxDataEapolDestination;
            break;

        case RX_DATA_PORT_STATUS_PARAM:
            pParamInfo->content.rxDataPortStatus = pRxData->rxDataPortStatus;
            break;

        case RX_DATA_COUNTERS_PARAM:
            pParamInfo->content.siteMgrTiWlanCounters.RecvOk = pRxData->rxDataCounters.RecvOk;              
            pParamInfo->content.siteMgrTiWlanCounters.DirectedBytesRecv = pRxData->rxDataCounters.DirectedBytesRecv;        
            pParamInfo->content.siteMgrTiWlanCounters.DirectedFramesRecv = pRxData->rxDataCounters.DirectedFramesRecv;      
            pParamInfo->content.siteMgrTiWlanCounters.MulticastBytesRecv = pRxData->rxDataCounters.MulticastBytesRecv;      
            pParamInfo->content.siteMgrTiWlanCounters.MulticastFramesRecv = pRxData->rxDataCounters.MulticastFramesRecv;    
            pParamInfo->content.siteMgrTiWlanCounters.BroadcastBytesRecv = pRxData->rxDataCounters.BroadcastBytesRecv;      
            pParamInfo->content.siteMgrTiWlanCounters.BroadcastFramesRecv = pRxData->rxDataCounters.BroadcastFramesRecv;    
            break;

        case RX_DATA_GET_RX_DATA_FILTERS_STATISTICS:
            TWD_ItrDataFilterStatistics (pRxData->hTWD, 
                                         pParamInfo->content.interogateCmdCBParams.fCb,
                                         pParamInfo->content.interogateCmdCBParams.hCb, 
                                         pParamInfo->content.interogateCmdCBParams.pCb);
            break;

        case RX_DATA_RATE_PARAM:
            pParamInfo->content.siteMgrCurrentRxRate = pRxData->uLastDataPktRate;
            break;

        default:
            TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, " rxData_getParam() : PARAMETER NOT SUPPORTED \n");
            return (PARAM_NOT_SUPPORTED);
    }

    return TI_OK;
}

/***************************************************************************
*                           rxData_setParam                                *
****************************************************************************
* DESCRIPTION:  set a specific parameter
* 
* INPUTS:       hRxData - the object
*               pParamInfo - structure which include the value to set for 
*               the requested parameter
*       
* OUTPUT:       
* 
* RETURNS:      TI_OK
*               TI_NOK
***************************************************************************/
TI_STATUS rxData_setParam(TI_HANDLE hRxData, paramInfo_t *pParamInfo)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    /* check handle validity */
    if( pRxData == NULL  )
    {
        return TI_NOK;
    }

    switch (pParamInfo->paramType)
    {
        case RX_DATA_EXCLUDE_UNENCRYPTED_PARAM:
            pRxData->rxDataExcludeUnencrypted = pParamInfo->content.rxDataExcludeUnencrypted;
            break;
        case RX_DATA_EXCLUDE_BROADCAST_UNENCRYPTED_PARAM:
            pRxData->rxDataExludeBroadcastUnencrypted = pParamInfo->content.rxDataExcludeUnencrypted;
            break;
        case RX_DATA_EAPOL_DESTINATION_PARAM:
            pRxData->rxDataEapolDestination = pParamInfo->content.rxDataEapolDestination;
            break;

        case RX_DATA_PORT_STATUS_PARAM:
            pRxData->rxDataPortStatus = pParamInfo->content.rxDataPortStatus;
            break;

        case RX_DATA_ENABLE_DISABLE_RX_DATA_FILTERS:
            return rxData_enableDisableRxDataFilters(hRxData, pParamInfo->content.rxDataFilterEnableDisable);

        case RX_DATA_ADD_RX_DATA_FILTER:
        {
            TRxDataFilterRequest* pRequest = &pParamInfo->content.rxDataFilterRequest;            

            return rxData_addRxDataFilter(hRxData, pRequest);
        }

        case RX_DATA_REMOVE_RX_DATA_FILTER:
        {
            TRxDataFilterRequest* pRequest = &pParamInfo->content.rxDataFilterRequest;
            
            return rxData_removeRxDataFilter(hRxData, pRequest);
        }

        case RX_DATA_GENERIC_ETHERTYPE_PARAM:
            pRxData->genericEthertype = pParamInfo->content.rxGenericEthertype;
            break;

        default:
            TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, " rxData_setParam() : PARAMETER NOT SUPPORTED \n");
            return (PARAM_NOT_SUPPORTED);
    }

    return TI_OK;
}

/***************************************************************************
*                     rxData_enableDisableRxDataFilters                    *
****************************************************************************
* DESCRIPTION:  
*               
* 
* INPUTS:       
*               
*               
*       
* OUTPUT:       
* 
* RETURNS:      
*               
***************************************************************************/
static TI_STATUS rxData_enableDisableRxDataFilters(TI_HANDLE hRxData, TI_BOOL enabled)
{
    rxData_t * pRxData = (rxData_t *) hRxData;

    /* assert 0 or 1 */
    if (enabled != TI_FALSE)
        enabled = 1;

    if (enabled == pRxData->filteringEnabled)
        return TI_OK;

    pRxData->filteringEnabled = enabled;

    return TWD_CfgEnableRxDataFilter (pRxData->hTWD, pRxData->filteringEnabled, pRxData->filteringDefaultAction);
}

/***************************************************************************
*                          findFilterRequest                               *
****************************************************************************
* DESCRIPTION:  
*               
* 
* INPUTS:       
*               
*               
*       
* OUTPUT:       
* 
* RETURNS:      
*               
***************************************************************************/
static int findFilterRequest(TI_HANDLE hRxData, TRxDataFilterRequest* request)
{
    rxData_t * pRxData = (rxData_t *) hRxData;
    int i;
    
    for (i = 0; i < MAX_DATA_FILTERS; ++i)
    {
        if (pRxData->isFilterSet[i])
        {
            if ((pRxData->filterRequests[i].offset == request->offset) &&
                (pRxData->filterRequests[i].maskLength == request->maskLength) &&
                (pRxData->filterRequests[i].patternLength == request->patternLength))
            {
                if ((os_memoryCompare(pRxData->hOs, pRxData->filterRequests[i].mask, request->mask, request->maskLength) == 0) &&
                    (os_memoryCompare(pRxData->hOs, pRxData->filterRequests[i].pattern, request->pattern, request->patternLength) == 0))
                    return i;
            }
        }
    }

    return -1;
}

/***************************************************************************
*                            closeFieldPattern                             *
****************************************************************************
* DESCRIPTION:  
*               
* 
* INPUTS:       
*               
*               
*       
* OUTPUT:       
* 
* RETURNS:      
*               
***************************************************************************/
static void closeFieldPattern (rxData_t * pRxData, rxDataFilterFieldPattern_t * fieldPattern, TI_UINT8 * fieldPatterns, TI_UINT8 * lenFieldPatterns)
{
    //fieldPatterns[*lenFieldPatterns] = fieldPattern->offset;
	os_memoryCopy(pRxData->hOs, fieldPatterns + *lenFieldPatterns, (TI_UINT8 *)&fieldPattern->offset, sizeof(fieldPattern->offset));
    *lenFieldPatterns += sizeof(fieldPattern->offset);

    fieldPatterns[*lenFieldPatterns] = fieldPattern->length;
    *lenFieldPatterns += sizeof(fieldPattern->length);

    fieldPatterns[*lenFieldPatterns] = fieldPattern->flag;
    *lenFieldPatterns += sizeof(fieldPattern->flag);

    os_memoryCopy(pRxData->hOs, fieldPatterns + *lenFieldPatterns, fieldPattern->pattern, fieldPattern->length);
    *lenFieldPatterns += fieldPattern->length;

    /* if the pattern bit mask is enabled add it to the end of the request */
    if ((fieldPattern->flag & RX_DATA_FILTER_FLAG_USE_BIT_MASK) == RX_DATA_FILTER_FLAG_USE_BIT_MASK)
    {
        os_memoryCopy(pRxData->hOs, fieldPatterns + *lenFieldPatterns, fieldPattern->mask, fieldPattern->length);
        *lenFieldPatterns += fieldPattern->length;
    }

    TRACE3(pRxData->hReport, REPORT_SEVERITY_INFORMATION, ": Closed field pattern, length = %d, total length = %d, pattern bit mask = %d.\n", fieldPattern->length, *lenFieldPatterns, ((fieldPattern->flag & RX_DATA_FILTER_FLAG_USE_BIT_MASK) == RX_DATA_FILTER_FLAG_USE_BIT_MASK));
}


/***************************************************************************
*                         parseRxDataFilterRequest                         *
****************************************************************************
* DESCRIPTION:  
*               
* 
* INPUTS:       
*               
*               
*       
* OUTPUT:       
* 
* RETURNS:      
*               
***************************************************************************/
static int parseRxDataFilterRequest(TI_HANDLE hRxData, TRxDataFilterRequest* request, TI_UINT8 * numFieldPatterns, TI_UINT8 * lenFieldPatterns, TI_UINT8 * fieldPatterns)
{
    rxData_t * pRxData = (rxData_t *) hRxData;

    int maskIter;
    int patternIter = 0;

    /* used to store field patterns while they are built */
    TI_BOOL isBuildingFieldPattern = TI_FALSE;
    rxDataFilterFieldPattern_t fieldPattern;

    for (maskIter = 0; maskIter < request->maskLength * 8; ++maskIter)
    {
        /* which byte in the mask and which bit in the byte we're at */
        int bit = maskIter % 8;
        int byte = maskIter / 8;

        /* is the bit in the mask set */
        TI_BOOL isSet = ((request->mask[byte] & (1 << bit)) == (1 << bit));

        TRACE4(pRxData->hReport, REPORT_SEVERITY_INFORMATION, ": MaskIter = %d, Byte = %d, Bit = %d, isSet = %d\n", maskIter, byte, bit, isSet);

        /* if we're in the midst of building a field pattern, we need to close in case */
        /* the current bit is not set or we've reached the ethernet header boundary */
        if (isBuildingFieldPattern)
        {
            if ((isSet == TI_FALSE) || (request->offset + maskIter == RX_DATA_FILTER_ETHERNET_HEADER_BOUNDARY))
            {
                closeFieldPattern(hRxData, &fieldPattern, fieldPatterns, lenFieldPatterns);

                isBuildingFieldPattern = TI_FALSE;
            }
        }

        /* nothing to do in case the bit is not set */
        if (isSet)
        {
            /* if not already building a field pattern, create a new one */
            if (isBuildingFieldPattern == TI_FALSE)
            {
                TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, ": Creating a new field pattern.\n");

                isBuildingFieldPattern = TI_TRUE;
                ++(*numFieldPatterns);

                if (*numFieldPatterns > RX_DATA_FILTER_MAX_FIELD_PATTERNS)
                {
                    TRACE1(pRxData->hReport, REPORT_SEVERITY_ERROR, ": Invalid filter, too many field patterns, maximum of %u is allowed!\n", RX_DATA_FILTER_MAX_FIELD_PATTERNS);

                    return TI_NOK;
                }

                fieldPattern.offset = request->offset + maskIter;
                fieldPattern.length = 0;

                /* we don't support the mask per bit feature yet. */
                fieldPattern.flag = RX_DATA_FILTER_FLAG_NO_BIT_MASK;

                /* first 14 bits are used for the Ethernet header, rest for the IP header */
                if (fieldPattern.offset < RX_DATA_FILTER_ETHERNET_HEADER_BOUNDARY)
                {
                    fieldPattern.flag |= RX_DATA_FILTER_FLAG_ETHERNET_HEADER;
                }
                else
                {
                    fieldPattern.flag |= RX_DATA_FILTER_FLAG_IP_HEADER;
                    fieldPattern.offset -= RX_DATA_FILTER_ETHERNET_HEADER_BOUNDARY;
                }

                TRACE2(pRxData->hReport, REPORT_SEVERITY_INFORMATION, ": offset = %d, flag = %d.\n", fieldPattern.offset, fieldPattern.flag);
            }

            /* check that the pattern is long enough */
            if (patternIter > request->patternLength)
            {
                TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, ": Invalid filter, mask and pattern length are not consistent!\n");

                return TI_NOK;
            }

            /* add the current pattern byte to the field pattern */
            fieldPattern.pattern[fieldPattern.length++] = request->pattern[patternIter++];

            /* check pattern matching boundary */
            if (fieldPattern.offset + fieldPattern.length >= RX_DATA_FILTER_FILTER_BOUNDARY)
            {
                TRACE1(pRxData->hReport, REPORT_SEVERITY_ERROR, ": Invalid filter, pattern matching cannot exceed first %u characters.\n", RX_DATA_FILTER_FILTER_BOUNDARY);

                return TI_NOK;
            }
        }
    }

    /* check that the pattern is long enough */
    if (patternIter != request->patternLength)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, ": Invalid filter, mask and pattern lengths are not consistent!\n");

        return TI_NOK;
    }

    /* close the last field pattern if needed */
    if (isBuildingFieldPattern)
    {
        closeFieldPattern (hRxData, &fieldPattern, fieldPatterns, lenFieldPatterns);
    }

    return TI_OK;
}


/***************************************************************************
*                           rxData_setRxDataFilter                         *
****************************************************************************
* DESCRIPTION:  
*               
* 
* INPUTS:       
*               
*               
*       
* OUTPUT:       
* 
* RETURNS:      
*               
***************************************************************************/
static TI_STATUS rxData_addRxDataFilter (TI_HANDLE hRxData, TRxDataFilterRequest* request)
{
    rxData_t * pRxData = (rxData_t *) hRxData;

    /* firmware request fields */
    TI_UINT8 index = 0;
    TI_UINT8 numFieldPatterns = 0;
    TI_UINT8 lenFieldPatterns = 0;
    TI_UINT8 fieldPatterns[MAX_DATA_FILTER_SIZE];

    /* does the filter already exist? */
    if (findFilterRequest(hRxData, request) >= 0)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, ": Filter already exists.\n");

        return RX_FILTER_ALREADY_EXISTS;
    }

    /* find place for insertion */
    for (index = 0; index < MAX_DATA_FILTERS; ++index)
    {
        if (pRxData->isFilterSet[index] == TI_FALSE)
            break;
    }

    /* are all filter slots taken? */
    if (index == MAX_DATA_FILTERS)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, ": No place to insert filter!\n");

        return RX_NO_AVAILABLE_FILTERS;
    }

    TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION, ": Inserting filter at index %d.\n", index);

    /* parse the filter request into discrete field patterns */
    if (parseRxDataFilterRequest(hRxData, request, &numFieldPatterns, &lenFieldPatterns, fieldPatterns) != TI_OK)
        return TI_NOK;

    if (numFieldPatterns == 0)
        return TI_NOK;

    /* Store configuration for future manipulation */
    pRxData->isFilterSet[index] = TI_TRUE;
    os_memoryCopy(pRxData->hOs, &pRxData->filterRequests[index], request, sizeof(pRxData->filterRequests[index]));

    /* Send configuration to firmware */
    return TWD_CfgRxDataFilter (pRxData->hTWD, 
                                index, 
                                ADD_FILTER, 
                                FILTER_SIGNAL, 
                                numFieldPatterns, 
                                lenFieldPatterns, 
                                fieldPatterns);

}

/***************************************************************************
*                         rxData_removeRxDataFilter                        *
****************************************************************************
* DESCRIPTION:  
*               
* 
* INPUTS:       
*               
*               
*       
* OUTPUT:       
* 
* RETURNS:      
*               
***************************************************************************/
static TI_STATUS rxData_removeRxDataFilter (TI_HANDLE hRxData, TRxDataFilterRequest* request)
{
    rxData_t * pRxData = (rxData_t *) hRxData;

    int index = findFilterRequest(hRxData, request);

    /* does the filter exist? */
    if (index < 0)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_WARNING, ": Remove data filter request received but the specified filter was not found!");

        return RX_FILTER_DOES_NOT_EXIST;
    }

    TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION, ": Removing filter at index %d.", index);

    pRxData->isFilterSet[index] = TI_FALSE;

    return TWD_CfgRxDataFilter (pRxData->hTWD, 
                                index, 
                                REMOVE_FILTER, 
                                FILTER_SIGNAL, 
                                0, 
                                0, 
                                NULL);
}

/***************************************************************************
*                       rxData_DistributorRxEvent                          *
****************************************************************************
* DESCRIPTION:  
*               
* 
* INPUTS:       
*               
*               
*       
* OUTPUT:       
* 
* RETURNS:      
*               
***************************************************************************/
static void rxData_DistributorRxEvent (rxData_t *pRxData, TI_UINT16 Mask, int DataLen)
{
    DistributorMgr_EventCall (pRxData->RxEventDistributor, Mask, DataLen);
}

/***************************************************************************
*                       rxData_RegNotif                                    *
****************************************************************************/
TI_HANDLE rxData_RegNotif (TI_HANDLE hRxData, TI_UINT16 EventMask, GeneralEventCall_t CallBack, TI_HANDLE context, TI_UINT32 Cookie)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    if (!hRxData)
        return NULL;

    return DistributorMgr_Reg (pRxData->RxEventDistributor, EventMask, (TI_HANDLE)CallBack, context, Cookie);
}

/***************************************************************************
*                       rxData_AddToNotifMask                              *
****************************************************************************/
TI_STATUS rxData_AddToNotifMask (TI_HANDLE hRxData, TI_HANDLE Notifh, TI_UINT16 EventMask)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    if (!hRxData)
        return TI_NOK;

    return DistributorMgr_AddToMask (pRxData->RxEventDistributor, Notifh, EventMask);
}


/***************************************************************************
*                       rxData_UnRegNotif                                  *
****************************************************************************/
TI_STATUS rxData_UnRegNotif(TI_HANDLE hRxData,TI_HANDLE RegEventHandle)
{
    rxData_t *pRxData = (rxData_t *)hRxData;
    
    if (!hRxData)
        return TI_NOK;

    return DistributorMgr_UnReg (pRxData->RxEventDistributor, RegEventHandle);
}


/***************************************************************************
*                       rxData_receivePacketFromWlan                       *
****************************************************************************
* DESCRIPTION:  this function is called by the GWSI for each received Buffer.
*               It filter and distribute the received Buffer. 
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/
void rxData_receivePacketFromWlan (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t        *pRxData    = (rxData_t *)hRxData;
    TMacAddr        address3;
    dot11_header_t  *pDot11Hdr;

    TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_receivePacketFromWlan() : pRxAttr->packetType = %d\n", pRxAttr->ePacketType);

    switch (pRxAttr->ePacketType)
    {
    case TAG_CLASS_MANAGEMENT:
    case TAG_CLASS_BCN_PRBRSP:

        TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION, "rxData_receivePacketFromWlan(): Received management Buffer len = %d\n", RX_BUF_LEN(pBuffer));

        /* update siteMngr 
         *
         * the BSSID in mgmt frames is always addr3 in the header 
         * must copy address3 since Buffer is freed in mlmeParser_recv
         */  
        pDot11Hdr = (dot11_header_t*)RX_BUF_DATA(pBuffer);
        
        os_memoryCopy(pRxData->hOs, &address3, &pDot11Hdr->address3, sizeof(address3));

        /* distribute mgmt pBuffer to mlme */
        if( mlmeParser_recv(pRxData->hMlme, pBuffer, pRxAttr) != TI_OK )
        {
            TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, " rxData_receivePacketFromWlan() : MLME returned error \n");
        }
        break;

    case TAG_CLASS_DATA:
    case TAG_CLASS_QOS_DATA:
    case TAG_CLASS_AMSDU:
    case TAG_CLASS_EAPOL:
        {
            CL_TRACE_START_L3();
            TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_receivePacketFromWlan() : Received Data Buffer len = %d\n", RX_BUF_LEN(pBuffer));

            /* send pBuffer to data dispatcher */
            rxData_dataPacketDisptcher(hRxData, pBuffer, pRxAttr);
            CL_TRACE_END_L3("tiwlan_drv.ko", "INHERIT", "RX", ".DataPacket");
            break;
        }

    default:
        TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_receivePacketFromWlan(): Received unspecified packet type !!! \n");
        RxBufFree(pRxData->hOs, pBuffer); 
        break;
    }
}

/***************************************************************************
*                       rxData_dataPacketDisptcher                         *
****************************************************************************
* DESCRIPTION:  this function is called upon receving data Buffer,
*               it dispatches the packet to the approciate function according to 
*               data packet type and rx port status. 
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/

static void rxData_dataPacketDisptcher (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t *pRxData = (rxData_t *)hRxData;
    portStatus_e DataPortStatus;
    rxDataPacketType_e DataPacketType;

    /* get rx port status */
    DataPortStatus = pRxData->rxDataPortStatus;

    /* discard data packets received while rx data port is closed */
    if (DataPortStatus == CLOSE)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_dataPacketDisptcher() : Received Data Buffer while Rx data port is closed \n");

        rxData_discardPacket (hRxData, pBuffer, pRxAttr);
        return;
    }

    /* get data packet type */

    pRxData->uLastDataPktRate = pRxAttr->Rate;  /* save Rx packet rate for statistics */

#ifdef XCC_MODULE_INCLUDED
    if (XCCMngr_isIappPacket (pRxData->hXCCMgr, pBuffer) == TI_TRUE)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_dataPacketDisptcher() : Received Iapp Buffer  \n");

        DataPacketType = DATA_IAPP_PACKET;

        /* dispatch Buffer according to packet type and current rx data port status */
        pRxData->rxData_dispatchBuffer[DataPortStatus][DataPacketType] (hRxData, pBuffer, pRxAttr);
    }
    else
#endif
    {
        /* A-MSDU ? */
        if (TAG_CLASS_AMSDU == pRxAttr->ePacketType)
        {
           rxData_ConvertAmsduToEthPackets (hRxData, pBuffer, pRxAttr);
        }
        else
        {
           TI_UINT16 etherType = 0;
           TEthernetHeader * pEthernetHeader;

           /*
            * if Host processes received packets, the header translation
            * from WLAN to ETH is done here. The conversion has been moved
            * here so that IAPP packets aren't converted.
            */
           rxData_convertWlanToEthHeader (hRxData, pBuffer, &etherType);

           pEthernetHeader = (TEthernetHeader *)RX_ETH_PKT_DATA(pBuffer);

           if (etherType == ETHERTYPE_802_1D)
           {
               TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_dataPacketDisptcher() : Received VLAN packet  \n");

               DataPacketType = DATA_VLAN_PACKET;
           }
           else if ((HTOWLANS(pEthernetHeader->type) == EAPOL_PACKET) ||
					(HTOWLANS(pEthernetHeader->type) == pRxData->genericEthertype))
           {
				TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_dataPacketDisptcher() : Received Eapol packet  \n");

				if (rxData_IsReAuthInProgress(pRxData))
				{
					/* ReAuth already in progress, restart timer */
					rxData_StopReAuthActiveTimer(pRxData);
					rxData_StartReAuthActiveTimer(pRxData);
				}
				else
				{
					if (PowerMgr_getReAuthActivePriority(pRxData->hPowerMgr))
					{
						/* ReAuth not in progress yet, force active, set flag, restart timer, send event */
						rxData_SetReAuthInProgress(pRxData, TI_TRUE);
						rxData_StartReAuthActiveTimer(pRxData);
						rxData_ReauthEnablePriority(pRxData);
						EvHandlerSendEvent(pRxData->hEvHandler, IPC_EVENT_RE_AUTH_STARTED, NULL, 0);
					}
				}

				DataPacketType = DATA_EAPOL_PACKET;
           }
           else
           {
               TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_dataPacketDisptcher() : Received Data packet  \n");

               DataPacketType = DATA_DATA_PACKET;
           }

           /* dispatch Buffer according to packet type and current rx data port status */
           pRxData->rxData_dispatchBuffer[DataPortStatus][DataPacketType] (hRxData, pBuffer, pRxAttr);
        }
    }

}

/***************************************************************************
*                       rxData_discardPacket                                   *
****************************************************************************
* DESCRIPTION:  this function is called to discard Buffer
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/
static void rxData_discardPacket (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    TRACE2(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_discardPacket: rx port status = %d , Buffer status = %d  \n", pRxData->rxDataPortStatus, pRxAttr->status);

    pRxData->rxDataDbgCounters.excludedFrameCounter++;

    /* free Buffer */
    RxBufFree(pRxData->hOs, pBuffer); 

}

/***************************************************************************
*                       rxData_discardPacketVlan                                   *
****************************************************************************
* DESCRIPTION:  this function is called to discard Buffer
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/
static void rxData_discardPacketVlan (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    TRACE0(pRxData->hReport, REPORT_SEVERITY_WARNING, " rxData_discardPacketVlan : drop packet that contains VLAN tag\n");

    pRxData->rxDataDbgCounters.rxDroppedDueToVLANIncludedCnt++;

    /* free Buffer */
    RxBufFree(pRxData->hOs, pBuffer); 
}


/***************************************************************************
*                       rxData_rcvPacketInOpenNotify                         *
****************************************************************************
* DESCRIPTION:  this function is called upon receving data Eapol packet type 
*               while rx port status is "open notify"
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/
static void rxData_rcvPacketInOpenNotify (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, " rxData_rcvPacketInOpenNotify: receiving data packet while in rx port status is open notify\n");

    pRxData->rxDataDbgCounters.rcvUnicastFrameInOpenNotify++;

    /* free Buffer */
    RxBufFree(pRxData->hOs, pBuffer); 
}


/***************************************************************************
*                       rxData_rcvPacketEapol                               *
****************************************************************************
* DESCRIPTION:  this function is called upon receving data Eapol packet type 
*               while rx port status is "open  eapol"
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/
static void rxData_rcvPacketEapol(TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_rcvPacketEapol() : Received an EAPOL frame tranferred to OS\n");

    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_rcvPacketEapol() : Received an EAPOL frame tranferred to OS\n");

    EvHandlerSendEvent (pRxData->hEvHandler, IPC_EVENT_EAPOL, NULL, 0);
    os_receivePacket (pRxData->hOs, (struct RxIfDescriptor_t*)pBuffer, pBuffer, (TI_UINT16)RX_ETH_PKT_LEN(pBuffer));

}

/***************************************************************************
*                       rxData_rcvPacketData                                 *
****************************************************************************
* DESCRIPTION:  this function is called upon receving data "data" packet type 
*               while rx port status is "open"
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/
static void rxData_rcvPacketData(TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t *pRxData = (rxData_t *)hRxData;
    TEthernetHeader *pEthernetHeader;
    TI_UINT16 EventMask = 0;
    TFwInfo *pFwInfo;

    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_rcvPacketData() : Received DATA frame tranferred to OS\n");

    /* check encryption status */
    pEthernetHeader = (TEthernetHeader *)RX_ETH_PKT_DATA(pBuffer);
    if (!MAC_MULTICAST (pEthernetHeader->dst))
    {  /* unicast frame */
        if((pRxData->rxDataExcludeUnencrypted) && (!(pRxAttr->packetInfo & RX_DESC_ENCRYPT_MASK)))
        {
            pRxData->rxDataDbgCounters.excludedFrameCounter++;
            /* free Buffer */
            TRACE0(pRxData->hReport, REPORT_SEVERITY_WARNING, " rxData_rcvPacketData() : exclude unicast unencrypted is TI_TRUE & packet encryption is OFF\n");

            RxBufFree(pRxData->hOs, pBuffer);
            return;
        }
    }
    else
    {  /* broadcast frame */
        if ((pRxData->rxDataExludeBroadcastUnencrypted) && (!(pRxAttr->packetInfo & RX_DESC_ENCRYPT_MASK)))
        {
            pRxData->rxDataDbgCounters.excludedFrameCounter++;
            /* free Buffer */
            TRACE0(pRxData->hReport, REPORT_SEVERITY_WARNING, " rxData_rcvPacketData() : exclude broadcast unencrypted is TI_TRUE & packet encryption is OFF\n");

            RxBufFree(pRxData->hOs, pBuffer);
            return;
        }

        /*
         * Discard multicast/broadcast frames that we sent ourselves.
         * Per IEEE 802.11-2007 section 9.2.7: "STAs shall filter out
         * broadcast/multicast messages that contain their address as
         * the source address."
         */
        pFwInfo = TWD_GetFWInfo (pRxData->hTWD);
        if (MAC_EQUAL(pFwInfo->macAddress, pEthernetHeader->src))
        {
            pRxData->rxDataDbgCounters.excludedFrameCounter++;
            /* free Buffer */
            RxBufFree(pRxData->hOs, pBuffer);
            return;
        }
    }

    /* update traffic monitor parameters */
    pRxData->rxDataCounters.RecvOk++;
    EventMask |= RECV_OK;

    if (!MAC_MULTICAST (pEthernetHeader->dst)) 
    {
        /* Directed frame */
        pRxData->rxDataCounters.DirectedFramesRecv++;
        pRxData->rxDataCounters.DirectedBytesRecv += RX_ETH_PKT_LEN(pBuffer);
        EventMask |= DIRECTED_BYTES_RECV;  
        EventMask |= DIRECTED_FRAMES_RECV;
    }
    else if (MAC_BROADCAST (pEthernetHeader->dst)) 
    {
        /* Broadcast frame */
        pRxData->rxDataCounters.BroadcastFramesRecv++;
        pRxData->rxDataCounters.BroadcastBytesRecv += RX_ETH_PKT_LEN(pBuffer);
        EventMask |= BROADCAST_BYTES_RECV;  
        EventMask |= BROADCAST_FRAMES_RECV;
    }
    else 
    {
        /* Multicast Address */
        pRxData->rxDataCounters.MulticastFramesRecv++;
        pRxData->rxDataCounters.MulticastBytesRecv += RX_ETH_PKT_LEN(pBuffer);
        EventMask |= MULTICAST_BYTES_RECV;  
        EventMask |= MULTICAST_FRAMES_RECV;
    }
    pRxData->rxDataCounters.LastSecBytesRecv += RX_ETH_PKT_LEN(pBuffer);

    /*Handle PREAUTH_EAPOL_PACKET*/
    if (HTOWLANS(pEthernetHeader->type) == PREAUTH_EAPOL_PACKET)
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_rcvPacketData() : Received an Pre-Auth EAPOL frame tranferred to OS\n");
    }

    rxData_DistributorRxEvent (pRxData, EventMask, RX_ETH_PKT_LEN(pBuffer));

    /* deliver packet to os */
    os_receivePacket (pRxData->hOs, (struct RxIfDescriptor_t*)pBuffer, pBuffer, (TI_UINT16)RX_ETH_PKT_LEN(pBuffer));
}


/***************************************************************************
*                       rxData_rcvPacketIapp                                 *
****************************************************************************
* DESCRIPTION:  this function is called upon receving data IAPP packet type 
*               while rx port status is "open"
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received Buffer.
*               pRxAttr - Rx attributes
*       
* OUTPUT:       
* 
* RETURNS:      
***************************************************************************/
#ifdef XCC_MODULE_INCLUDED

static void rxData_rcvPacketIapp(TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_rcvPacketIapp() : Received IAPP frame tranferred to XCCMgr\n");

    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_rcvPacketIapp() : Received IAPP frame tranferred to XCCMgr\n");

    /* tranfer packet to XCCMgr */
    XCCMngr_recvIAPPPacket (pRxData->hXCCMgr, pBuffer, pRxAttr);

    /* free Buffer */
    RxBufFree(pRxData->hOs, pBuffer); 
}

#endif


/****************************************************************************
*                       rxData_convertWlanToEthHeader                       *
*****************************************************************************
* DESCRIPTION:  this function convert the Packet header from 802.11 header 
*               format to ethernet format
* 
* INPUTS:       hRxData - the object
*               pBuffer - the received pBuffer in 802.11 format
*       
* OUTPUT:       pEthPacket  - pointer to the received pBuffer in ethernet format
*               uEthLength - ethernet packet length
* 
* RETURNS:      TI_OK/TI_NOK
***************************************************************************/
static TI_STATUS rxData_convertWlanToEthHeader (TI_HANDLE hRxData, void *pBuffer, TI_UINT16 * etherType)
{
    TEthernetHeader      EthHeader;
    Wlan_LlcHeader_T    *pWlanSnapHeader;   
    TI_UINT8            *dataBuf;
    dot11_header_t      *pDot11Header;
    TI_UINT32            lengthDelta;
    TI_UINT16            swapedTypeLength;
    TI_UINT32            headerLength;
    TI_UINT8             createEtherIIHeader;
    rxData_t *pRxData = (rxData_t *)hRxData;

    dataBuf = (TI_UINT8 *)RX_BUF_DATA(pBuffer);

    /* Setting the mac header len according to the received FrameControl field in the Mac Header */
    GET_MAX_HEADER_SIZE (dataBuf, &headerLength);
    pDot11Header = (dot11_header_t*) dataBuf;
    pWlanSnapHeader = (Wlan_LlcHeader_T*)((TI_UINT32)dataBuf + (TI_UINT32)headerLength);
    
    swapedTypeLength = WLANTOHS (pWlanSnapHeader->Type);
    *etherType = swapedTypeLength;
 
    /* Prepare the Ethernet header. */
    if( ENDIAN_HANDLE_WORD(pDot11Header->fc) & DOT11_FC_FROM_DS)
    {   /* Infrastructure  bss */
        MAC_COPY (EthHeader.dst, pDot11Header->address1);
        MAC_COPY (EthHeader.src, pDot11Header->address3);
    }
    else
    {   /* Independent bss */
        MAC_COPY (EthHeader.dst, pDot11Header->address1);
        MAC_COPY (EthHeader.src, pDot11Header->address2);
    }
    
    createEtherIIHeader = TI_FALSE;
	/* See if the LLC header in the frame shows the SAP SNAP... */
	if((SNAP_CHANNEL_ID == pWlanSnapHeader->DSAP) &&
	   (SNAP_CHANNEL_ID == pWlanSnapHeader->SSAP) &&
	   (LLC_CONTROL_UNNUMBERED_INFORMATION == pWlanSnapHeader->Control))
	{
		/* Check for the Bridge Tunnel OUI in the SNAP Header... */
		if((SNAP_OUI_802_1H_BYTE0 == pWlanSnapHeader->OUI[ 0 ]) &&
		   (SNAP_OUI_802_1H_BYTE1 == pWlanSnapHeader->OUI[ 1 ]) &&
		   (SNAP_OUI_802_1H_BYTE2 == pWlanSnapHeader->OUI[ 2 ]))
		{
			/* Strip the SNAP header by skipping over it.                  */
			/* Start moving data from the Ethertype field in the SNAP      */
			/* header.  Move to the TypeLength field in the 802.3 header.  */
			createEtherIIHeader = TI_TRUE;
		}
		/* Check for the RFC 1042 OUI in the SNAP Header   */
		else
		{
			/* Check for the RFC 1042 OUI in the SNAP Header   */
			if(	(SNAP_OUI_RFC1042_BYTE0 == pWlanSnapHeader->OUI[ 0 ]) &&
				(SNAP_OUI_RFC1042_BYTE1 == pWlanSnapHeader->OUI[ 1 ]) &&
				(SNAP_OUI_RFC1042_BYTE2 == pWlanSnapHeader->OUI[ 2 ]))
			{
			/* See if the Ethertype is in our selective translation table  */
			/* (Appletalk AARP and DIX II IPX are the two protocols in     */
			/* our 'table')                                                */
				if((ETHERTYPE_APPLE_AARP != swapedTypeLength) &&
					(ETHERTYPE_DIX_II_IPX != swapedTypeLength))
			{
				/* Strip the SNAP header by skipping over it. */
				createEtherIIHeader = TI_TRUE;
			}
		}
	}
    }

    if( createEtherIIHeader == TI_TRUE )
    {
    /* The LEN/TYPE bytes are set to TYPE, the entire WLAN+SNAP is removed.*/
    lengthDelta = headerLength + WLAN_SNAP_HDR_LEN - ETHERNET_HDR_LEN;
    EthHeader.type = pWlanSnapHeader->Type;
    }
    else
    {
	/* The LEN/TYPE bytes are set to frame LEN, only the WLAN header is removed, */
	/* the entire 802.3 or 802.2 header is not removed.*/
	lengthDelta = headerLength - ETHERNET_HDR_LEN;
	EthHeader.type = WLANTOHS((TI_UINT16)(RX_BUF_LEN(pBuffer) - headerLength));
    }
    
    /* Replace the 802.11 header and the LLC with Ethernet packet. */
    dataBuf += lengthDelta;
    os_memoryCopy (pRxData->hOs, dataBuf, (void*)&EthHeader, ETHERNET_HDR_LEN);
    RX_ETH_PKT_DATA(pBuffer) = dataBuf;
    RX_ETH_PKT_LEN(pBuffer)  = RX_BUF_LEN(pBuffer) - lengthDelta;

    return TI_OK;
}


/**
 * \brief convert A-MSDU to several ethernet packets
 * 
 * \param hRxData - the object
 * \param pBuffer - the received Buffer in A-MSDU 802.11n format
 * \param pRxAttr - Rx attributes
 * \return TI_OK on success or TI_NOK on failure 
 * 
 * \par Description
 * Static function
 * This function convert the A-MSDU Packet from A-MSDU 802.11n packet
 * format to several ethernet packets format and pass them to the OS layer
 *
 * \sa
 */ 
static TI_STATUS rxData_ConvertAmsduToEthPackets (TI_HANDLE hRxData, void *pBuffer, TRxAttr* pRxAttr)
{

    TEthernetHeader     *pMsduEthHeader;
    TEthernetHeader     *pEthHeader;
    Wlan_LlcHeader_T    *pWlanSnapHeader;   
    TI_UINT8            *pAmsduDataBuf;
    TI_UINT16            uAmsduDataLen;
    void                *pDataBuf;
    TI_UINT16            uDataLen;
    TI_UINT32            lengthDelta;
    TI_UINT16            swapedTypeLength;
    TI_UINT32            headerLength;
    rxDataPacketType_e   DataPacketType;
    rxData_t            *pRxData = (rxData_t *)hRxData;

    /* total AMPDU header */
    pAmsduDataBuf = (TI_UINT8 *)RX_BUF_DATA(pBuffer);
    /* Setting the mac header len according to the received FrameControl field in the Mac Header */
    GET_MAX_HEADER_SIZE (pAmsduDataBuf, &headerLength);
    
    /* 
     * init loop setting 
     */
    /* total AMPDU size */
    uAmsduDataLen = (TI_UINT16)(RX_BUF_LEN(pBuffer) - headerLength);
    /* ETH header */
    pMsduEthHeader = (TEthernetHeader *)(pAmsduDataBuf + headerLength);
    /* ETH length, in A-MSDU the MSDU header type contain the MSDU length and not the type */
    uDataLen = WLANTOHS(pMsduEthHeader->type);

    TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION, "rxData_ConvertAmsduToEthPackets(): A-MSDU received in length %d \n",uAmsduDataLen);

    /* if we have another packet at the AMSDU */
    while((uDataLen < uAmsduDataLen) && (uAmsduDataLen > ETHERNET_HDR_LEN + FCS_SIZE))  
    {
        /* allocate a new buffer */
        /* RxBufAlloc() add an extra word for alignment the MAC payload */
        rxData_RequestForBuffer (hRxData, &pDataBuf, sizeof(RxIfDescriptor_t) + WLAN_SNAP_HDR_LEN + ETHERNET_HDR_LEN + uDataLen, 0, TAG_CLASS_AMSDU);
        if (NULL == pDataBuf)
        {
            TRACE1(pRxData->hReport, REPORT_SEVERITY_ERROR, "rxData_ConvertAmsduToEthPackets(): cannot alloc MSDU packet. length %d \n",uDataLen);
            rxData_discardPacket (hRxData, pBuffer, pRxAttr);
            return TI_NOK;
        }

        /* read packet type from LLC */
        pWlanSnapHeader = (Wlan_LlcHeader_T*)((TI_UINT8*)pMsduEthHeader + ETHERNET_HDR_LEN);
        swapedTypeLength = WLANTOHS (pWlanSnapHeader->Type);

        /* copy the RxIfDescriptor */
        os_memoryCopy (pRxData->hOs, pDataBuf, pBuffer, sizeof(RxIfDescriptor_t));

        /* update length, in the RxIfDescriptor the Len in words (4B) */
        ((RxIfDescriptor_t *)pDataBuf)->length = (sizeof(RxIfDescriptor_t) + WLAN_SNAP_HDR_LEN + ETHERNET_HDR_LEN + uDataLen) >> 2;
        ((RxIfDescriptor_t *)pDataBuf)->extraBytes = 4 - ((sizeof(RxIfDescriptor_t) + WLAN_SNAP_HDR_LEN + ETHERNET_HDR_LEN + uDataLen) & 0x3);

        /* Prepare the Ethernet header pointer. */ 
        /* add padding in the start of the buffer in order to align ETH payload */
        pEthHeader = (TEthernetHeader *)((TI_UINT8 *)(RX_BUF_DATA(pDataBuf)) + 
                                         WLAN_SNAP_HDR_LEN + 
                                         PADDING_ETH_PACKET_SIZE);

        /* copy the Ethernet header */
        os_memoryCopy (pRxData->hOs, pEthHeader, pMsduEthHeader, ETHERNET_HDR_LEN);

        /* The LEN/TYPE bytes are set to TYPE */
        pEthHeader->type = pWlanSnapHeader->Type;

        /* Delta length for the next packet */
        lengthDelta = ETHERNET_HDR_LEN + uDataLen;

        /* copy the packet payload */
        if (uDataLen > WLAN_SNAP_HDR_LEN)
            os_memoryCopy (pRxData->hOs,
                       (((TI_UINT8*)pEthHeader) + ETHERNET_HDR_LEN), 
                       ((TI_UINT8*)pMsduEthHeader) + ETHERNET_HDR_LEN + WLAN_SNAP_HDR_LEN, 
                       uDataLen - WLAN_SNAP_HDR_LEN);

        /* set the packet type */
        if (swapedTypeLength == ETHERTYPE_802_1D)
        {
            TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_ConvertAmsduToEthPackets() : Received VLAN Buffer  \n");

            DataPacketType = DATA_VLAN_PACKET;
        }
        else if (HTOWLANS(pEthHeader->type) == EAPOL_PACKET)
        {
            TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_ConvertAmsduToEthPackets() : Received Eapol pBuffer  \n");

            DataPacketType = DATA_EAPOL_PACKET;
        }
        else
        {
            TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, " rxData_ConvertAmsduToEthPackets() : Received Data pBuffer  \n");

            DataPacketType = DATA_DATA_PACKET;
        }

        /* update buffer setting */
        /* save the ETH packet address */
        RX_ETH_PKT_DATA(pDataBuf) = pEthHeader;
        /* save the ETH packet size */
        RX_ETH_PKT_LEN(pDataBuf) = uDataLen + ETHERNET_HDR_LEN - WLAN_SNAP_HDR_LEN;

        /* star of MSDU packet always align acceding to 11n spec */
        lengthDelta = (lengthDelta + ALIGN_4BYTE_MASK) & ~ALIGN_4BYTE_MASK;
        pMsduEthHeader = (TEthernetHeader *)(((TI_UINT8*)pMsduEthHeader) + lengthDelta);

        if(uAmsduDataLen > lengthDelta)
        {
            /* swich to the next MSDU */
            uAmsduDataLen = uAmsduDataLen - lengthDelta;

            /* Clear the EndOfBurst flag for all packets except the last one */
            ((RxIfDescriptor_t *)pDataBuf)->driverFlags &= ~DRV_RX_FLAG_END_OF_BURST;
        }
        else
        {
            /* no more MSDU */
            uAmsduDataLen = 0;
        }


        /* in A-MSDU the MSDU header type contain the MSDU length and not the type */
        uDataLen = WLANTOHS(pMsduEthHeader->type);


        /* dispatch Buffer according to packet type and current rx data port status */
        pRxData->rxData_dispatchBuffer[pRxData->rxDataPortStatus][DataPacketType] (hRxData, pDataBuf, pRxAttr);

    } /* while end */


    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION, "rxData_ConvertAmsduToEthPackets(): A-MSDU Packe conversion done.\n");

    /* free the A-MSDU packet */
    RxBufFree(pRxData->hOs, pBuffer);

    return TI_OK;
}

/****************************************************************************************
 *                        rxData_ReceivePacket                                              *
 ****************************************************************************************
DESCRIPTION:    receive packet CB from RxXfer.
                parse the status and other parameters and forward the frame to  
                rxData_receivePacketFromWlan()
                
INPUT:          Rx frame with its parameters    

OUTPUT:

RETURN:         

************************************************************************/
static void rxData_ReceivePacket (TI_HANDLE   hRxData,
                                  void  *pBuffer)
{
    rxData_t            *pRxData    = (rxData_t *)hRxData;

    TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION, "rxData_ReceivePacket: Received BUF %x\n", pBuffer);

    if (pBuffer)
    {
        RxIfDescriptor_t    *pRxParams  = (RxIfDescriptor_t*)pBuffer;
        TRxAttr             RxAttr; 
        ERate               appRate;
        dot11_header_t      *pHdr;

        /*
         * First thing we do is getting the dot11_header, and than we check the status, since the header is
         * needed for RX_MIC_FAILURE_ERROR 
         */

        pHdr = (dot11_header_t *)RX_BUF_DATA(pBuffer);

        /* Check status */
        switch (pRxParams->status & RX_DESC_STATUS_MASK)
        {
            case RX_DESC_STATUS_SUCCESS:
                break;
            
            case RX_DESC_STATUS_DECRYPT_FAIL:
            {
                /* This error is not important before the Connection, so we ignore it when portStatus is not OPEN */
                if (pRxData->rxDataPortStatus == OPEN) 
                {
                    TRACE0(pRxData->hReport, REPORT_SEVERITY_WARNING, "rxData_ReceivePacket: Received Packet with RX_DESC_DECRYPT_FAIL\n");
                }
            
                RxBufFree(pRxData->hOs, pBuffer); 
                return;
            }
            case RX_DESC_STATUS_MIC_FAIL:
            {
                TI_UINT8 uKeyType; 
                paramInfo_t Param;
                TMacAddr* pMac = &pHdr->address1; /* hold the first mac address */
                /* Get BSS type */
                Param.paramType = SITE_MGR_CURRENT_BSS_TYPE_PARAM;
                siteMgr_getParam (pRxData->hSiteMgr, &Param);
     
                /* For multicast/broadcast frames or in IBSS the key used is GROUP, else - it's Pairwise */
                if (MAC_MULTICAST(*pMac) || Param.content.siteMgrCurrentBSSType == BSS_INDEPENDENT)
                {
                    uKeyType = (TI_UINT8)KEY_TKIP_MIC_GROUP;
	                TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, "rxData_ReceivePacket: Received Packet MIC failure. Type = Group\n");
                }
                /* Unicast on infrastructure */
                else 
                {
                    uKeyType = (TI_UINT8)KEY_TKIP_MIC_PAIRWISE;
	                TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, "rxData_ReceivePacket: Received Packet MIC failure. Type = Pairwise\n");
                }
    
    
                rsn_reportMicFailure (pRxData->hRsn, &uKeyType, sizeof(uKeyType));
                RxBufFree(pRxData->hOs, pBuffer); 
                return;
            }
    
            case RX_DESC_STATUS_DRIVER_RX_Q_FAIL:
            {
                /* Rx queue error - free packet and return */
                TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR, "rxData_ReceivePacket: Received Packet with Rx queue error \n");
  
                RxBufFree(pRxData->hOs, pBuffer); 
                return;
            }

            default:    
                    /* Unknown error - free packet and return */
                    TRACE1(pRxData->hReport, REPORT_SEVERITY_ERROR, "rxData_ReceivePacket: Received Packet with unknown status = %d\n", (pRxParams->status & RX_DESC_STATUS_MASK));
    
                    RxBufFree(pRxData->hOs, pBuffer); 
                    return;
            }

        TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION , "Receive good Packet\n");
        
        if (rate_PolicyToDrv ((ETxRateClassId)(pRxParams->rate), &appRate) != TI_OK)
        {
            TRACE1(pRxData->hReport, REPORT_SEVERITY_ERROR , "rxData_ReceivePacket: can't convert hwRate=0x%x\n", pRxParams->rate);
        }
        
        /*
         * Set rx attributes 
         */ 
        RxAttr.channel    = pRxParams->channel;
        RxAttr.packetInfo = pRxParams->flags;
        RxAttr.ePacketType= pRxParams->packet_class_tag;
        RxAttr.Rate       = appRate;
        RxAttr.Rssi       = pRxParams->rx_level;
        RxAttr.SNR        = pRxParams->rx_snr;
        RxAttr.status     = pRxParams->status & RX_DESC_STATUS_MASK;
        /* for now J band not implemented */
        RxAttr.band       = ((pRxParams->flags & RX_DESC_BAND_MASK) == RX_DESC_BAND_A) ? 
                            RADIO_BAND_5_0_GHZ : RADIO_BAND_2_4_GHZ ;
        RxAttr.eScanTag   = (EScanResultTag)(pRxParams->proccess_id_tag);
        /* timestamp is 32 bit so do bytes copy to avoid exception in case the RxInfo is in 2 bytes offset */
        os_memoryCopy (pRxData->hOs,
                       (void *)&(RxAttr.TimeStamp), 
                       (void *)&(pRxParams->timestamp), 
                       sizeof(pRxParams->timestamp) );
        RxAttr.TimeStamp = ENDIAN_HANDLE_LONG(RxAttr.TimeStamp);

        TRACE8(pRxData->hReport, REPORT_SEVERITY_INFORMATION, "rxData_ReceivePacket: channel=%d, info=0x%x, type=%d, rate=0x%x, RSSI=%d, SNR=%d, status=%d, scan tag=%d\n", RxAttr.channel, RxAttr.packetInfo, RxAttr.ePacketType, RxAttr.Rate, RxAttr.Rssi, RxAttr.SNR, RxAttr.status, RxAttr.eScanTag);

        rxData_receivePacketFromWlan (hRxData, pBuffer, &RxAttr);

        /* 
         *  Buffer MUST be freed until now 
         */
    }
    else
    {
        TRACE0(pRxData->hReport, REPORT_SEVERITY_ERROR , "rxData_ReceivePacket: null Buffer received");
    }
}


/****************************************************************************************
 *                        rxData_RequestForBuffer                                              *
 ****************************************************************************************
DESCRIPTION:     RX request for buffer
                uEncryptionflag API are for GWSI use.
INPUT:          

OUTPUT:

RETURN:         

************************************************************************/
static ERxBufferStatus rxData_RequestForBuffer (TI_HANDLE   hRxData,
                                      void **pBuf,
                                      TI_UINT16 aLength, 
                                      TI_UINT32 uEncryptionflag,
                                      PacketClassTag_e ePacketClassTag)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION , " RequestForBuffer, length = %d \n",aLength);

    *pBuf = RxBufAlloc (pRxData->hOs, aLength, ePacketClassTag);

    if (*pBuf)
    {
        return RX_BUF_ALLOC_COMPLETE;
    }
    else 
    {
        return RX_BUF_ALLOC_OUT_OF_MEM;
    }
}


/*******************************************************************
*                        DEBUG FUNCTIONS                           *
*******************************************************************/

#ifdef TI_DBG

/***************************************************************************
*                        rxData_resetCounters                              *
****************************************************************************
* DESCRIPTION:  This function reset the Rx Data module counters
*
* INPUTS:       hRxData - the object
*
* OUTPUT:       
* 
* RETURNS:      void
***************************************************************************/
void rxData_resetCounters(TI_HANDLE hRxData)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    os_memoryZero(pRxData->hOs, &pRxData->rxDataCounters, sizeof(rxDataCounters_t));
}

/***************************************************************************
*                        rxData_resetDbgCounters                           *
****************************************************************************
* DESCRIPTION:  This function reset the Rx Data module debug counters
*
* INPUTS:       hRxData - the object
*
* OUTPUT:       
* 
* RETURNS:      void
***************************************************************************/

void rxData_resetDbgCounters(TI_HANDLE hRxData)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    os_memoryZero(pRxData->hOs, &pRxData->rxDataDbgCounters, sizeof(rxDataDbgCounters_t));
}


/***************************************************************************
*                            test functions                                *
***************************************************************************/
void rxData_printRxCounters (TI_HANDLE hRxData)
{
#ifdef REPORT_LOG
    rxData_t *pRxData = (rxData_t *)hRxData;

    if (pRxData) 
    {
        WLAN_OS_REPORT(("RecvOk = %d\n", pRxData->rxDataCounters.RecvOk));
        WLAN_OS_REPORT(("DirectedBytesRecv = %d\n", pRxData->rxDataCounters.DirectedBytesRecv));
        WLAN_OS_REPORT(("DirectedFramesRecv = %d\n", pRxData->rxDataCounters.DirectedFramesRecv));
        WLAN_OS_REPORT(("MulticastBytesRecv = %d\n", pRxData->rxDataCounters.MulticastBytesRecv));
        WLAN_OS_REPORT(("MulticastFramesRecv = %d\n", pRxData->rxDataCounters.MulticastFramesRecv));
        WLAN_OS_REPORT(("BroadcastBytesRecv = %d\n", pRxData->rxDataCounters.BroadcastBytesRecv));
        WLAN_OS_REPORT(("BroadcastFramesRecv = %d\n", pRxData->rxDataCounters.BroadcastFramesRecv));
    
        /* debug counters */
        WLAN_OS_REPORT(("excludedFrameCounter = %d\n", pRxData->rxDataDbgCounters.excludedFrameCounter));
        WLAN_OS_REPORT(("rxDroppedDueToVLANIncludedCnt = %d\n", pRxData->rxDataDbgCounters.rxDroppedDueToVLANIncludedCnt));    
        WLAN_OS_REPORT(("rxWrongBssTypeCounter = %d\n", pRxData->rxDataDbgCounters.rxWrongBssTypeCounter));
        WLAN_OS_REPORT(("rxWrongBssIdCounter = %d\n", pRxData->rxDataDbgCounters.rxWrongBssIdCounter));
        WLAN_OS_REPORT(("rcvUnicastFrameInOpenNotify = %d\n", pRxData->rxDataDbgCounters.rcvUnicastFrameInOpenNotify));        
    }
#endif
}


void rxData_printRxBlock(TI_HANDLE hRxData)
{
#ifdef REPORT_LOG
    rxData_t *pRxData = (rxData_t *)hRxData;

    WLAN_OS_REPORT(("hCtrlData = 0x%X\n", pRxData->hCtrlData));
    WLAN_OS_REPORT(("hMlme = 0x%X\n", pRxData->hMlme));
    WLAN_OS_REPORT(("hOs = 0x%X\n", pRxData->hOs));
    WLAN_OS_REPORT(("hReport = 0x%X\n", pRxData->hReport));
    WLAN_OS_REPORT(("hRsn = 0x%X\n", pRxData->hRsn));
    WLAN_OS_REPORT(("hSiteMgr = 0x%X\n", pRxData->hSiteMgr));

    WLAN_OS_REPORT(("hCtrlData = 0x%X\n", pRxData->hCtrlData));
    WLAN_OS_REPORT(("hMlme = 0x%X\n", pRxData->hMlme));
    WLAN_OS_REPORT(("hOs = 0x%X\n", pRxData->hOs));
    WLAN_OS_REPORT(("hReport = 0x%X\n", pRxData->hReport));
    WLAN_OS_REPORT(("hRsn = 0x%X\n", pRxData->hRsn));
    WLAN_OS_REPORT(("hSiteMgr = 0x%X\n", pRxData->hSiteMgr));

    WLAN_OS_REPORT(("rxDataPortStatus = %d\n", pRxData->rxDataPortStatus));
    WLAN_OS_REPORT(("rxDataExcludeUnencrypted = %d\n", pRxData->rxDataExcludeUnencrypted));
    WLAN_OS_REPORT(("rxDataEapolDestination = %d\n", pRxData->rxDataEapolDestination));
#endif
}


void rxData_startRxThroughputTimer (TI_HANDLE hRxData)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    if (!pRxData->rxThroughputTimerEnable)
    {
        /* reset throughput counter */
        pRxData->rxDataCounters.LastSecBytesRecv = 0;
        pRxData->rxThroughputTimerEnable = TI_TRUE;

        /* start 1 sec throughput timer */
        tmr_StartTimer (pRxData->hThroughputTimer,
                        rxData_printRxThroughput,
                        (TI_HANDLE)pRxData,
                        1000,
                        TI_TRUE);
    }
}


void rxData_stopRxThroughputTimer (TI_HANDLE hRxData)
{

    rxData_t *pRxData = (rxData_t *)hRxData;

    if (pRxData->rxThroughputTimerEnable)
    {
        tmr_StopTimer (pRxData->hThroughputTimer);
        pRxData->rxThroughputTimerEnable = TI_FALSE;
    } 
}


static void rxData_printRxThroughput (TI_HANDLE hRxData, TI_BOOL bTwdInitOccured)
{
    rxData_t *pRxData = (rxData_t *)hRxData;

    WLAN_OS_REPORT (("\n"));
    WLAN_OS_REPORT (("-------------- Rx Throughput Statistics ---------------\n"));
    WLAN_OS_REPORT (("Throughput = %d KBits/sec\n", pRxData->rxDataCounters.LastSecBytesRecv * 8 / 1024));

    /* reset throughput counter */
    pRxData->rxDataCounters.LastSecBytesRecv = 0;
}

void rxData_printRxDataFilter (TI_HANDLE hRxData)
{
    TI_UINT32 index;
    rxData_t *pRxData = (rxData_t *)hRxData;

    for (index=0; index<MAX_DATA_FILTERS; index++)
     {
        if (pRxData->isFilterSet[index])
        {
            WLAN_OS_REPORT (("index=%d, pattern & mask\n", index));
            report_PrintDump(pRxData->filterRequests[index].pattern, pRxData->filterRequests[index].patternLength);
            report_PrintDump(pRxData->filterRequests[index].mask, pRxData->filterRequests[index].maskLength);
        }
        else
        {
            WLAN_OS_REPORT (("No Filter defined for index-%d\n", index));
        }
     }
}

#endif /*TI_DBG*/

/****************************************************************************
 *                      rxData_SetReAuthInProgress()
 ****************************************************************************
 * DESCRIPTION:	Sets the ReAuth flag value
 *
 * INPUTS: hRxData - the object
 *		   value - value to set the flag to
 *
 * OUTPUT:	None
 *
 * RETURNS:	OK or NOK
 ****************************************************************************/
void rxData_SetReAuthInProgress(TI_HANDLE hRxData, TI_BOOL	value)
{
	rxData_t *pRxData = (rxData_t *)hRxData;

	TRACE1(pRxData->hReport, REPORT_SEVERITY_INFORMATION , "Set ReAuth flag to %d\n", value);

	pRxData->reAuthInProgress = value;
}

/****************************************************************************
 *                      rxData_IsReAuthInProgress()
 ****************************************************************************
 * DESCRIPTION:	Returns the ReAuth flag value
 *
 * INPUTS: hRxData - the object
 *
 * OUTPUT:	None
 *
 * RETURNS:	ReAuth flag value
 ****************************************************************************/
TI_BOOL rxData_IsReAuthInProgress(TI_HANDLE hRxData)
{
	rxData_t *pRxData = (rxData_t *)hRxData;
	return pRxData->reAuthInProgress;
}

/****************************************************************************
*						rxData_StartReAuthActiveTimer   	                *
*****************************************************************************
* DESCRIPTION:	this function starts the ReAuthActive timer
*
* INPUTS:		hRxData - the object
*
* OUTPUT:		None
*
* RETURNS:		None
***************************************************************************/
static void rxData_StartReAuthActiveTimer(TI_HANDLE hRxData)
{
	rxData_t *pRxData = (rxData_t *)hRxData;
    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION , "Start ReAuth Active Timer\n");
    tmr_StartTimer (pRxData->reAuthActiveTimer,
                    reAuthTimeout,
                    (TI_HANDLE)pRxData,
                    pRxData->reAuthActiveTimeout,
                    TI_FALSE);
}

/****************************************************************************
*						rxData_StopReAuthActiveTimer   						*
*****************************************************************************
* DESCRIPTION:	this function stops the ReAuthActive timer
*
* INPUTS:		hRxData - the object
*
* OUTPUT:		None
*
* RETURNS:		None
***************************************************************************/
void rxData_StopReAuthActiveTimer(TI_HANDLE hRxData)
{
	rxData_t *pRxData = (rxData_t *)hRxData;
    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION , "Stop ReAuth Active Timer\n");
    tmr_StopTimer (pRxData->reAuthActiveTimer);
}

/****************************************************************************
*						reAuthTimeout   									*
*****************************************************************************
* DESCRIPTION:	this function ia called when the ReAuthActive timer elapses
*				It resets the Reauth flag and restore the PS state.
*				It also sends RE_AUTH_TERMINATED event to upper layer.
*
* INPUTS:		hRxData - the object
*
* OUTPUT:		None
*
* RETURNS:		None
***************************************************************************/
static void reAuthTimeout(TI_HANDLE hRxData, TI_BOOL bTwdInitOccured)
{
	rxData_t *pRxData = (rxData_t *)hRxData;

    TRACE0(pRxData->hReport, REPORT_SEVERITY_INFORMATION , "ReAuth Active Timeout\n");
	rxData_SetReAuthInProgress(pRxData, TI_FALSE);
	rxData_ReauthDisablePriority(pRxData);
    EvHandlerSendEvent(pRxData->hEvHandler, IPC_EVENT_RE_AUTH_TERMINATED, NULL, 0);
}

void rxData_ReauthEnablePriority(TI_HANDLE hRxData)
{
	rxData_t *pRxData = (rxData_t *)hRxData;
	paramInfo_t param;

    param.paramType = POWER_MGR_ENABLE_PRIORITY;
    param.content.powerMngPriority = POWER_MANAGER_REAUTH_PRIORITY;
    powerMgr_setParam(pRxData->hPowerMgr,&param);
}

void rxData_ReauthDisablePriority(TI_HANDLE hRxData)
{
	rxData_t *pRxData = (rxData_t *)hRxData;
    paramInfo_t param;

    param.paramType = POWER_MGR_DISABLE_PRIORITY;
    param.content.powerMngPriority = POWER_MANAGER_REAUTH_PRIORITY;
    powerMgr_setParam(pRxData->hPowerMgr,&param);
}
