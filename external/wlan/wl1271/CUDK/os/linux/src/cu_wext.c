/*
 * cu_wext.c
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
*   MODULE:  CU_Wext.c
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
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <string.h>
#include <stdlib.h>

#include "TWDriver.h"
#include "STADExternalIf.h"
#include "ParsEvent.h"
#include "ipc_sta.h"
#include "cu_os.h"

/* defines */
/***********/

/* local types */
/***************/
/* Module control block */
typedef struct
{
    THandle hIpcSta;
    union iwreq_data req_data;

    struct iw_scan_req *scan_req;   
    U16 scan_flag;
} TCuWext;

/* local variables */
/*******************/

/* local fucntions */
/*******************/
static S32 CuWext_FillBssidList(struct iw_event *iwe, OS_802_11_BSSID_EX* bssidList, S32 index)
{
    S32 res = 0;    
    switch(iwe->cmd)
    {
        case SIOCGIWAP:
            os_memcpy(bssidList[index].MacAddress, iwe->u.ap_addr.sa_data, MAC_ADDR_LEN);
            bssidList[index].Configuration.BeaconPeriod = 0; /* default configuration */
            res = 1;
            break;
        case SIOCGIWESSID:          
            bssidList[index-1].Ssid.SsidLength = iwe->u.data.length;
            os_memcpy(bssidList[index-1].Ssid.Ssid, iwe->u.data.pointer, bssidList[index-1].Ssid.SsidLength);
            if(iwe->u.data.length != MAX_SSID_LEN)
                bssidList[index-1].Ssid.Ssid[bssidList[index-1].Ssid.SsidLength] = 0;
            break;
        case SIOCGIWNAME:
            {
                unsigned i;
                S8 buffer[IFNAMSIZ];
                static const char *ieee80211_modes[] = {
                    "?", 
                    "IEEE 802.11 B", 
                    "IEEE 802.11 A", 
                    "IEEE 802.11 BG", 
                    "IEEE 802.11 ABG" };
                    
                os_memset(buffer, 0, IFNAMSIZ);
                os_memcpy(buffer, iwe->u.name, IFNAMSIZ);
                for(i=0;i<SIZE_ARR(ieee80211_modes); i++)
                    if (0 == os_strcmp((PS8)ieee80211_modes[i], buffer))
                        break;
                bssidList[index-1].NetworkTypeInUse = i;  
            }
            break;
        case SIOCGIWMODE:
            if(iwe->u.mode == IW_MODE_ADHOC)
                bssidList[index-1].InfrastructureMode = os802_11IBSS;
            else if (iwe->u.mode == IW_MODE_INFRA)
                bssidList[index-1].InfrastructureMode = os802_11Infrastructure;
            else if (iwe->u.mode == IW_MODE_AUTO)
                bssidList[index-1].InfrastructureMode = os802_11AutoUnknown;
            else
                bssidList[index-1].InfrastructureMode = os802_11InfrastructureMax;
            
            break;
        case SIOCGIWFREQ:
            bssidList[index-1].Configuration.Union.channel = iwe->u.freq.m;
            break;
        case IWEVQUAL:
            bssidList[index-1].Rssi = (S8)iwe->u.qual.level;
            break;
        case SIOCGIWENCODE:
            if(iwe->u.data.flags == (IW_ENCODE_ENABLED | IW_ENCODE_NOKEY))
            {
                bssidList[index-1].Privacy = TRUE;
                bssidList[index-1].Capabilities |= CAP_PRIVACY_MASK<<CAP_PRIVACY_SHIFT;
            }
            else
            {
                bssidList[index-1].Privacy = FALSE;
                bssidList[index-1].Capabilities &= ~(CAP_PRIVACY_MASK<<CAP_PRIVACY_SHIFT);
            }
            break;
        case SIOCGIWRATE:
            break;
        case IWEVCUSTOM:
            {
                S8 buffer[100];
                
                os_memset(buffer, 0, 100);
                os_memcpy(buffer, iwe->u.data.pointer, iwe->u.data.length);

                if(!os_strncmp(buffer, (PS8)"Bcn", 3))
                {
                    char *p1;
                    p1 = strtok(&buffer[10], " ");
                    bssidList[index-1].Configuration.BeaconPeriod = atoi(p1);
                }
            }           
            break;          
    }

    return res;
}


/* functions */
/*************/

THandle CuOs_Create(THandle hIpcSta)
{
    TCuWext* pCuWext = (TCuWext*)os_MemoryCAlloc(sizeof(TCuWext), sizeof(U8));
    if(pCuWext == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuOs_Create - cant allocate control block\n");
        return NULL;
    }

    pCuWext->hIpcSta = hIpcSta;

    return pCuWext;
}

VOID CuOs_Destroy(THandle hCuWext)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;

    os_MemoryFree(pCuWext);
}

S32 CuOs_Get_SSID(THandle hCuWext, OS_802_11_SSID* ssid)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res;

    os_memset(ssid->Ssid, 0, sizeof(OS_802_11_SSID) - sizeof(U32));

    pCuWext->req_data.essid.pointer = (PVOID)ssid->Ssid;
    pCuWext->req_data.essid.length = sizeof(OS_802_11_SSID) - sizeof(U32);  
    pCuWext->req_data.essid.flags = 0;

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWESSID, &pCuWext->req_data, sizeof(struct iw_point));
    if(res != OK)   
        return res;

    ssid->SsidLength = pCuWext->req_data.essid.length;

    return OK;
}

S32 CuOs_Get_BSSID(THandle hCuWext, TMacAddr bssid)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;   
    S32 res,i;

    os_memset(&pCuWext->req_data.ap_addr, 0x00, sizeof(struct sockaddr));

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWAP, &pCuWext->req_data, sizeof(struct sockaddr));
    if(res != OK)   
        return res;

    for(i=0;i<MAC_ADDR_LEN;i++)
        bssid[i] = pCuWext->req_data.ap_addr.sa_data[i];    

    return OK;
}

S32 CuOs_GetCurrentChannel(THandle hCuWext, U32* channel)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res;

    pCuWext->req_data.freq.m = 0;
    
    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWFREQ, &pCuWext->req_data, sizeof(struct iw_freq    ));
    if(res != OK)   
        return res;

    *channel = pCuWext->req_data.freq.m;

    return OK;
}

/*Usage example of SIOCGIWSTATS. This WEXT is used by wireless tools such as iwconfig, iwlib etc.*/
S32 CuOs_GetCurrentStats(THandle hCuWext, S32* rssi)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res;
	struct	iw_statistics stat;
	*rssi = 0;

	pCuWext->req_data.data.pointer = &stat;

	res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWSTATS, &pCuWext->req_data, sizeof(struct iw_statistics));
    if(res != OK)
        return res;

    *rssi = stat.qual.level;

    return OK;
}

S32 CuOs_Start_Scan(THandle hCuWext, OS_802_11_SSID* ssid, U8 scanType)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    struct iw_scan_req tReq;
    S32 res;

    if (ssid->SsidLength > IW_ESSID_MAX_SIZE) 
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuOs_Start_Scan - too long SSID (%lu)\n",ssid->SsidLength);
        return OSAL_ERROR;
    }

    if (ssid->Ssid[0] && ssid->SsidLength) 
    {
        os_memset(&tReq, 0, sizeof(tReq));
        tReq.essid_len = ssid->SsidLength;
        /*
         * tReq.bssid.sa_family = ARPHRD_ETHER;
         * os_memset(tReq.bssid.sa_data, 0xff, ETH_ALEN); 
         */
        os_memcpy(tReq.essid, ssid->Ssid, ssid->SsidLength);
        pCuWext->scan_req = &tReq;
        pCuWext->req_data.data.flags = IW_SCAN_THIS_ESSID;
    }
    else
    {
        pCuWext->req_data.data.flags = 0;
    }

	tReq.scan_type = scanType;
    pCuWext->req_data.data.pointer = &tReq;
    pCuWext->req_data.data.length = sizeof(struct iw_scan_req);

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCSIWSCAN, &pCuWext->req_data, sizeof(struct iw_point));
    if(res != OK)   
        return res;

    return OK;  
}

S32 CuOs_GetBssidList(THandle hCuWext, OS_802_11_BSSID_LIST_EX *bssidList)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res, NumberOfItems;
        
    /* allocate the scan result buffer */
    U8* buffer = os_MemoryCAlloc(IW_SCAN_MAX_DATA, sizeof(U8));
    if(buffer == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - CuOs_Get_BssidList - cant allocate scan result buffer\n");
        return EOALERR_CU_WEXT_ERROR_CANT_ALLOCATE;
    }

    NumberOfItems = 0;
    pCuWext->req_data.data.pointer = buffer;
    pCuWext->req_data.data.flags = 0;
    do
    {       
        pCuWext->req_data.data.length = IW_SCAN_MAX_DATA;       

        res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWSCAN, &pCuWext->req_data, sizeof(struct iw_point));
        if(res != OK)   
        {
            os_MemoryFree(buffer);
            return res;
        }
    
        /* parse the scan results */
        if(pCuWext->req_data.data.length)    
        {      
            struct iw_event     iwe;      
            struct stream_descr stream;                 
            S32                 ret;  
            
            /* init the event stream */
            os_memset((char *)&stream, '\0', sizeof(struct stream_descr));
            stream.current = (char *)buffer;
            stream.end = (char *)(buffer + pCuWext->req_data.data.length);

            do  
            {     
                /* Extract an event and print it */   
                ret = ParsEvent_GetEvent(&stream, &iwe);     
                if(ret > 0)     
                    NumberOfItems += CuWext_FillBssidList(&iwe, bssidList->Bssid, NumberOfItems);   
            }      
            while(ret > 0);      
        }       
        
    } while(pCuWext->req_data.data.flags);

    bssidList->NumberOfItems = NumberOfItems;   

    /* free the scan result buffer */
    os_MemoryFree(buffer);
    
    return OK;
}


S32 CuOs_Set_BSSID(THandle hCuWext, TMacAddr bssid)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;   
    S32 res;

    os_memcpy(pCuWext->req_data.ap_addr.sa_data, bssid, MAC_ADDR_LEN);
        
    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCSIWAP, &pCuWext->req_data, sizeof(struct sockaddr));
                                                
    if(res != OK)   
        return res;

    return OK;
}

S32 CuOs_Set_ESSID(THandle hCuWext, OS_802_11_SSID* ssid)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;   
    S32 res;

    pCuWext->req_data.essid.pointer = (PVOID)ssid->Ssid;
    pCuWext->req_data.essid.length = ssid->SsidLength;
    if(ssid->SsidLength)
        pCuWext->req_data.essid.flags = 1;  
    else
        pCuWext->req_data.essid.flags = 0;
    pCuWext->req_data.essid.flags |= SET_SSID_WITHOUT_SUPPL;

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCSIWESSID, &pCuWext->req_data, sizeof(struct sockaddr));
                                                
    if(res != OK)   
        return res;

    return OK;
}

S32 CuOs_GetTxPowerLevel(THandle hCuWext, S32* pTxPowerLevel)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;   
    S32 res;

    os_memset(&pCuWext->req_data.txpower, 0, sizeof(struct iw_param));

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWTXPOW, &pCuWext->req_data, sizeof(struct iw_param));
                                                
    if(res != OK)   
        return res;

    *pTxPowerLevel = pCuWext->req_data.txpower.value;

    return OK;
}

S32 CuOs_SetTxPowerLevel(THandle hCuWext, S32 txPowerLevel)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;   
    S32 res;

    os_memset(&pCuWext->req_data.txpower, 0, sizeof(struct iw_param));

    pCuWext->req_data.txpower.value = txPowerLevel;

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCSIWTXPOW, &pCuWext->req_data, sizeof(struct iw_param));
                                                
    if(res != OK)   
        return res;

    return OK;
}

S32 CuOs_GetRtsTh(THandle hCuWext, PS32 pRtsTh)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res;

    os_memset(&pCuWext->req_data.rts, 0, sizeof(struct iw_param));

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWRTS, &pCuWext->req_data, sizeof(struct iw_param));
    if(res != OK)   
        return res;

    *pRtsTh = pCuWext->req_data.rts.value;

    return OK;
}


S32 CuOs_SetRtsTh(THandle hCuWext, S32 RtsTh)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res;

    os_memset(&pCuWext->req_data.rts, 0, sizeof(struct iw_param));
    pCuWext->req_data.rts.value = RtsTh;

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCSIWRTS, &pCuWext->req_data, sizeof(struct iw_param));
    if(res != OK)   
        return res;

    

    return OK;
}

S32 CuOs_GetFragTh(THandle hCuWext, PS32 pFragTh)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res;

    os_memset(&pCuWext->req_data.frag, 0, sizeof(struct iw_param));

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCGIWFRAG, &pCuWext->req_data, sizeof(struct iw_param));
    if(res != OK)   
        return res;

    *pFragTh = pCuWext->req_data.frag.value;

    return OK;
}

S32 CuOs_SetFragTh(THandle hCuWext, S32 FragTh)
{
    TCuWext* pCuWext = (TCuWext*)hCuWext;
    S32 res;

    os_memset(&pCuWext->req_data.frag, 0, sizeof(struct iw_param));
    pCuWext->req_data.frag.value = FragTh;

    res = IPC_STA_Wext_Send(pCuWext->hIpcSta, SIOCSIWFRAG, &pCuWext->req_data, sizeof(struct iw_param));
    if(res != OK)   
        return res; 

    return OK;
}
/*stab function (should be filled later on for Linux to get ThreadID of the WLAN driver*/
S32 CuOs_GetDriverThreadId(THandle hCuWext, U32* threadid)
{
	return OK;
}
