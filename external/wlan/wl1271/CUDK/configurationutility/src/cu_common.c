/*
 * cu_common.c
 *
 * Copyright 2001-2009 Texas Instruments, Inc. - http://www.ti.com/
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and  
 * limitations under the License.
 */

/****************************************************************************
*
*   MODULE:  CU_Common.c
*   
*   PURPOSE: 
* 
*   DESCRIPTION:  
*   ============
*      
*
****************************************************************************/

/* includes */
/************/
#include "cu_osapi.h"
#include "oserr.h"

#include "TWDriver.h"
#include "convert.h"

#include "ipc_sta.h"
#include "cu_common.h"

/* defines */
/***********/

/* local types */
/***************/
/* Module control block */
typedef struct CuCommon_t
{
    THandle hIpcSta;
} CuCommon_t;


typedef enum
{
    DRIVER_STATUS_IDLE              = 0,
    DRIVER_STATUS_RUNNING           = 1
} PARAM_OUT_Driver_Status_e;


/* local variables */
/*******************/

/* local fucntions */
/*******************/


/* functions */
/*************/
THandle CuCommon_Create(THandle *pIpcSta, const PS8 device_name)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)os_MemoryCAlloc(sizeof(CuCommon_t), sizeof(U8));
    if(pCuCommon == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)("ERROR - CuCommon_Create - cant allocate control block\n") );
        return NULL;
    }

    pCuCommon->hIpcSta = IpcSta_Create(device_name);
    if(pCuCommon->hIpcSta == NULL)
    {   
        CuCommon_Destroy(pCuCommon);
        return NULL;
    }
    *pIpcSta = pCuCommon->hIpcSta;

    return pCuCommon;
}

VOID CuCommon_Destroy(THandle hCuCommon)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;

    if(pCuCommon->hIpcSta)
        IpcSta_Destroy(pCuCommon->hIpcSta);

    os_MemoryFree(pCuCommon);
}

S32 CuCommon_SetU32(THandle hCuCommon, U32 PrivateIoctlId, U32 Data)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, &Data, sizeof(U32), 
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}

S32 CuCommon_GetU32(THandle hCuCommon, U32 PrivateIoctlId, PU32 pData)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, NULL, 0, 
                                                pData, sizeof(U32));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}

S32 CuCommon_SetU16(THandle hCuCommon, U32 PrivateIoctlId, U16 Data)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, &Data, sizeof(U16), 
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}

S32 CuCommon_SetU8(THandle hCuCommon, U32 PrivateIoctlId, U8 Data)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, &Data, sizeof(U8), 
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}


S32 CuCommon_GetU8(THandle hCuCommon, U32 PrivateIoctlId, PU8 pData)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, NULL, 0, 
                                                pData, sizeof(U8));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}


S32 CuCommon_SetBuffer(THandle hCuCommon, U32 PrivateIoctlId, PVOID pBuffer, U32 len)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon; 
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, pBuffer, len, 
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;      
}

S32 CuCommon_GetBuffer(THandle hCuCommon, U32 PrivateIoctlId, PVOID pBuffer, U32 len)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon; 
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, NULL, 0, 
                                                pBuffer, len);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;      
}

S32 CuCommon_GetSetBuffer(THandle hCuCommon, U32 PrivateIoctlId, PVOID pBuffer, U32 len)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon; 
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, PrivateIoctlId, pBuffer, len, 
                                                pBuffer, len);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;      
}
   
S32 CuCommon_Get_BssidList_Size(THandle hCuCommon, PU32 pSizeOfBssiList)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon; 
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, SCAN_CNCN_BSSID_LIST_SIZE_PARAM, NULL, 0, 
                                                pSizeOfBssiList, sizeof(U32));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;      
}

S32 CuCommon_GetRssi(THandle hCuCommon, PS8 pdRssi, PS8 pbRssi)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon; 
    S32 res;    
    TCuCommon_RoamingStatisticsTable buffer;
    
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_802_11_RSSI, NULL, 0, 
                                                &buffer, sizeof(TCuCommon_RoamingStatisticsTable));
    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    *pdRssi = (S8)buffer.rssi;
	*pbRssi = (S8)buffer.rssiBeacon;

    return OK;
}

S32 CuCommon_GetSnr(THandle hCuCommon, PU32 pdSnr, PU32 pbSnr)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon; 
    S32 res;
    TCuCommon_RoamingStatisticsTable buffer;
        
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TWD_SNR_RATIO_PARAM, NULL, 0, 
                                                &buffer, sizeof(TCuCommon_RoamingStatisticsTable));
    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

	*pdSnr = (U32)buffer.snr;
	*pbSnr = (U32)buffer.snrBeacon;

    return OK;
}

S32 CuCommon_GetTxStatistics(THandle hCuCommon, TIWLN_TX_STATISTICS* pTxCounters, U32 doReset)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon; 
    S32 res;

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_802_11_TX_STATISTICS, pTxCounters, sizeof(TIWLN_TX_STATISTICS),
                                                pTxCounters, sizeof(TIWLN_TX_STATISTICS));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    if(doReset)
    {
        res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TX_CTRL_RESET_COUNTERS_PARAM, NULL, 0, 
                                                NULL, 0);
        if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
            return ECUERR_CU_COMMON_ERROR;
    }

    return OK;      
}

S32 CuCommon_Radio_Test(THandle hCuCommon,TTestCmd* data)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;

	res = IPC_STA_Private_Send(pCuCommon->hIpcSta, 
							   TWD_RADIO_TEST_PARAM, 
							   (PVOID)data, 
							   sizeof(TTestCmd),
							   (PVOID)data, 
							   sizeof(TTestCmd));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
	{
		os_error_printf(CU_MSG_INFO2, (PS8)"In CuCommon_Radio_Test: IPC_STA_Private_Send failed\n");            
		return ECUERR_CU_COMMON_ERROR;
	}

    return OK;  
}

S32 CuCommon_AddKey(THandle hCuCommon, OS_802_11_WEP* pKey)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;    
    OS_802_11_KEY  key;

    os_memset(&key, 0, sizeof(OS_802_11_KEY));

    key.Length = pKey->Length;
    key.KeyIndex = (pKey->KeyIndex & 0x80000000) | (pKey->KeyIndex & 0x3FFFFFFF);
    key.KeyLength = pKey->KeyLength;
    os_memcpy(key.KeyMaterial, pKey->KeyMaterial, pKey->KeyLength);

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, RSN_ADD_KEY_PARAM, &key, sizeof(OS_802_11_KEY), 
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}

S32 CuCommon_RemoveKey(THandle hCuCommon, U32 KeyIndex)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;
    OS_802_11_KEY  key;

    os_memset(&key, 0, sizeof(OS_802_11_KEY));
    key.KeyIndex = KeyIndex;
    
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, RSN_REMOVE_KEY_PARAM, &key, sizeof(OS_802_11_KEY), 
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}

S32 CuCommon_GetDfsChannels(THandle hCuCommon, PU16 pMinDfsChannel, PU16 pMaxDfsChannel)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;
    DFS_ChannelRange_t DFS_ChannelRange;
    
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_REG_DOMAIN_GET_DFS_RANGE, NULL, 0, 
                                                &DFS_ChannelRange, sizeof(DFS_ChannelRange_t));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    *pMaxDfsChannel = DFS_ChannelRange.maxDFS_channelNum;
    *pMinDfsChannel = DFS_ChannelRange.minDFS_channelNum;

    return OK;  
}

S32 CuCommon_SetDfsChannels(THandle hCuCommon, U16 MinDfsChannel, U16 MaxDfsChannel)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;
    DFS_ChannelRange_t DFS_ChannelRange;

    DFS_ChannelRange.maxDFS_channelNum = MaxDfsChannel;
    DFS_ChannelRange.minDFS_channelNum = MinDfsChannel;
    
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_REG_DOMAIN_SET_DFS_RANGE, &DFS_ChannelRange, sizeof(DFS_ChannelRange_t),
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}

S32 CuCommon_PrintDriverDebug(THandle hCuCommon, PVOID pParams, U32 param_size)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;

	if ( pParams == NULL )
	{
		return ECUERR_CU_COMMON_ERROR;
	}

    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_DISPLAY_STATS, pParams, param_size,
							   NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}

S32 CuCommon_PrintDriverDebugBuffer(THandle hCuCommon, U32 func_id, U32 opt_param)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res, len;
    U8 buf[260];  /* no more then 256 + func id */

    if (opt_param == 0)
        return ECUERR_CU_ERROR;

    len = os_strlen((PS8)opt_param);
    *(PU32)buf = func_id;
    os_memcpy((PS8)buf + sizeof(U32),(PS8)opt_param, len);
        
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_DISPLAY_STATS, buf, len + sizeof(U32),
                                                NULL, 0);

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    return OK;  
}


S32 CuCommon_GetRxDataFiltersStatistics(THandle hCuCommon, PU32 pUnmatchedPacketsCount, PU32 pMatchedPacketsCount)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    S32 res;
    TCuCommon_RxDataFilteringStatistics buffer;
    
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_GET_RX_DATA_FILTERS_STATISTICS, NULL, 0,
                                                &buffer, sizeof(TCuCommon_RxDataFilteringStatistics));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    *pUnmatchedPacketsCount = buffer.unmatchedPacketsCount;
    os_memcpy(pMatchedPacketsCount, &buffer.matchedPacketsCount, MAX_DATA_FILTERS*sizeof(U32));

    return OK;  
}


S32 CuCommon_GetPowerConsumptionStat(THandle hCuCommon, ACXPowerConsumptionTimeStat_t *pPowerstat)
{
    CuCommon_t* pCuCommon = (CuCommon_t*)hCuCommon;
    ACXPowerConsumptionTimeStat_t tStatistics;
    S32 res;

    
    res = IPC_STA_Private_Send(pCuCommon->hIpcSta, TIWLN_GET_POWER_CONSUMPTION_STATISTICS, NULL, 0,
                                                &tStatistics, sizeof(ACXPowerConsumptionTimeStat_t));

    if(res == EOALERR_IPC_STA_ERROR_SENDING_WEXT)
        return ECUERR_CU_COMMON_ERROR;

    os_memcpy(pPowerstat, &tStatistics, sizeof(ACXPowerConsumptionTimeStat_t));

    return OK;  
}







