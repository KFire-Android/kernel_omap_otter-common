/*
 * ipc_event.c
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
*   MODULE:  IPC_Event.c
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
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/rtnetlink.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/wireless.h>
#include "cu_osapi.h"
#include "oserr.h"

#include "TWDriver.h"
#include "STADExternalIf.h"

#include "ParsEvent.h"
#include "ipc_event.h"

/* defines */
/***********/
#define PIPE_READ 0
#define PIPE_WRITE 1
#define IPC_EVENT_KEY ((key_t)123456789)

/* IPC evemt messages to child */
#define IPC_EVENT_MSG_KILL                  "IPC_EVENT_MSG_KILL"
#define IPC_EVENT_MSG_UPDATE_DEBUG_LEVEL    "IPC_EVENT_MSG_UPDATE_DEBUG_LEVEL"
#define IPC_EVENT_MSG_MAX_LEN   50

/* local types */
/***************/
typedef struct IpcEvent_Shared_Memory_t
{
    int pipe_fields[2];
    u64 event_mask;
    union 
    {
        S32 debug_level;
    } content;
} IpcEvent_Shared_Memory_t;

/* Module control block */
typedef struct IpcEvent_t
{
    IpcEvent_Shared_Memory_t* p_shared_memory;
    S32 child_process_id;
    S32 pipe_to_child;
} IpcEvent_t;

typedef struct IpcEvent_Child_t
{
    S32 STA_socket;
    IpcEvent_Shared_Memory_t* p_shared_memory;
    S32 pipe_from_parent;    
} IpcEvent_Child_t;

/* local variables */
/*******************/
VOID g_tester_send_event(U8 event_index);
/* local fucntions */
/*******************/
static VOID IpcEvent_SendMessageToChild(IpcEvent_t* pIpcEvent, PS8 msg)
{
    write(pIpcEvent->pipe_to_child, msg, os_strlen(msg));    
}

static S32 IpcEvent_Sockets_Open(VOID)
{
    S32 skfd;
    struct sockaddr_nl  local;

    skfd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (skfd < 0) 
    {
        return -1;
    }

    os_memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_groups = RTMGRP_LINK;
    if (bind(skfd, (struct sockaddr *) &local, sizeof(local)) < 0) 
    {
        close(skfd);
        return -2;
    }
    
    return skfd;
}

static inline VOID IpcEvent_Sockets_Close(S32 skfd)
{
    close(skfd);
}

void ProcessLoggerMessage(PU8 data, U16 len);

static VOID IpcEvent_PrintEvent(IpcEvent_Child_t* pIpcEventChild, U32 EventId, TI_UINT8* pData, S32 DataLen)
{

    if(pIpcEventChild->p_shared_memory->event_mask & ((u64)1<<EventId))
    {
        switch(EventId)
        {
            case IPC_EVENT_DISASSOCIATED:
            {
                OS_802_11_DISASSOCIATE_REASON_T    *pDisAssoc;

                if (NULL == pData)
                {
                    return;
                }
                else
                {
                    pDisAssoc = (OS_802_11_DISASSOCIATE_REASON_T*)pData;
                }

                switch(pDisAssoc->eDisAssocType)
                {
                    case OS_DISASSOC_STATUS_UNSPECIFIED:
                        os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated with unspecified reason (User/SG/Recovery)\n");
                        break;
                    case OS_DISASSOC_STATUS_AUTH_REJECT:
                        if (pDisAssoc->uStatusCode == STATUS_PACKET_REJ_TIMEOUT)
                        {
                            os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to no Auth response \n");
                        } 
                        else
                        {
                            os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to Auth response packet with reason = %d\n", pDisAssoc->uStatusCode);
                        }
                        break;
                    case OS_DISASSOC_STATUS_ASSOC_REJECT:
                        if (pDisAssoc->uStatusCode == STATUS_PACKET_REJ_TIMEOUT)
                        {
                            os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to no Assoc response \n");
                        } 
                        else
                        {
                            os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to Assoc response packet with reason = %d\n", pDisAssoc->uStatusCode);
                        }
                        break;
                    case OS_DISASSOC_STATUS_SECURITY_FAILURE:
                        os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to RSN failure\n");
                        break;
                    case OS_DISASSOC_STATUS_AP_DEAUTHENTICATE:
                        os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to AP deAuthenticate packet with reason = %d\n", pDisAssoc->uStatusCode);
                        break;
                    case OS_DISASSOC_STATUS_AP_DISASSOCIATE:
                        os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to AP disAssoc packet with reason = %d\n", pDisAssoc->uStatusCode);
                        break;
                    case OS_DISASSOC_STATUS_ROAMING_TRIGGER:
                        os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated due to roaming trigger = %d\n", pDisAssoc->uStatusCode);
                        break;
                    default:
                        os_error_printf(CU_MSG_ERROR, "CLI Event - Disassociated with unknown reason = %d\n", pDisAssoc->eDisAssocType);
                        break; 
                }
                
                break; /* the end of the IPC_EVENT_DISASSOCIATED case */
            }
            case IPC_EVENT_ASSOCIATED:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_ASSOCIATED\n");
                break;
            case IPC_EVENT_MEDIA_SPECIFIC:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_MEDIA_SPECIFIC\n");
                break;
            case IPC_EVENT_SCAN_COMPLETE:
				os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_SCAN_COMPLETE\n");
                break;
            /* custom events */
            case IPC_EVENT_SCAN_STOPPED:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_SCAN_STOPPED\n");
                break;
            case IPC_EVENT_LINK_SPEED:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_LINK_SPEED\n");
                break;
            case IPC_EVENT_AUTH_SUCC:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_AUTH_SUCC\n");
                break;
            case IPC_EVENT_CCKM_START:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_CCKM_START\n");
                break;
            case IPC_EVENT_EAPOL:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_EAPOL\n");
                break;
            case IPC_EVENT_RE_AUTH_STARTED:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_RE_AUTH_STARTED\n");
                break;
            case IPC_EVENT_RE_AUTH_COMPLETED:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_RE_AUTH_COMPLETED\n");
                break;
            case IPC_EVENT_RE_AUTH_TERMINATED:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_RE_AUTH_TERMINATED\n");
                break;
            case IPC_EVENT_BOUND:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_BOUND\n");
                break;
            case IPC_EVENT_UNBOUND:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_UNBOUND\n");
                break;
            case IPC_EVENT_PREAUTH_EAPOL:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_PREAUTH_EAPOL\n");
                break;
            case IPC_EVENT_LOW_RSSI:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_LOW_RSSI\n");
                break;
            case IPC_EVENT_TSPEC_STATUS:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_TSPEC_STATUS\n");

                OS_802_11_QOS_TSPEC_PARAMS* tspec = (OS_802_11_QOS_TSPEC_PARAMS*)pData;
                os_error_printf(CU_MSG_ERROR, "CLI Event - IPC_EVENT_TSPEC_STATUS -- (ReasonCode = %d) \n",tspec->uReasonCode);
                os_error_printf(CU_MSG_ERROR, "Tspec Parameters (as received through event handler):\n");
                os_error_printf(CU_MSG_ERROR, "-----------------------------------------------------\n");
                os_error_printf(CU_MSG_ERROR, "userPriority = %d\n",tspec->uUserPriority);
                os_error_printf(CU_MSG_ERROR, "uNominalMSDUsize = %d\n",tspec->uNominalMSDUsize);
                os_error_printf(CU_MSG_ERROR, "uMeanDataRate = %d\n",tspec->uMeanDataRate);
                os_error_printf(CU_MSG_ERROR, "uMinimumPHYRate = %d\n",tspec->uMinimumPHYRate);
                os_error_printf(CU_MSG_ERROR, "uSurplusBandwidthAllowance = %d\n",tspec->uSurplusBandwidthAllowance);
                os_error_printf(CU_MSG_ERROR, "uAPSDFlag = %d\n",tspec->uAPSDFlag);
                os_error_printf(CU_MSG_ERROR, "MinimumServiceInterval = %d\n",tspec->uMinimumServiceInterval);
                os_error_printf(CU_MSG_ERROR, "MaximumServiceInterval = %d\n",tspec->uMaximumServiceInterval);
                os_error_printf(CU_MSG_ERROR, "uMediumTime = %d\n\n",tspec->uMediumTime);
           
                break;
            case IPC_EVENT_TSPEC_RATE_STATUS:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_TSPEC_RATE_STATUS\n");
                break;
            case IPC_EVENT_MEDIUM_TIME_CROSS:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_MEDIUM_TIME_CROSS\n");
                break;
            case IPC_EVENT_ROAMING_COMPLETE:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_ROAMING_COMPLETE\n");
                break;
            case IPC_EVENT_EAP_AUTH_FAILURE:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_EAP_AUTH_FAILURE\n");
                break;
            case IPC_EVENT_WPA2_PREAUTHENTICATION:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_WPA2_PREAUTHENTICATION\n");
                break;
            case IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED:
            {
                U32 *crossInfo = (U32 *)pData;
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_TRAFFIC_INTENSITY_THRESHOLD_CROSSED\n");
                os_error_printf(CU_MSG_ERROR, (PS8)"Threshold(High=0,  Low=1)   crossed= %d\n", crossInfo[0]);
                os_error_printf(CU_MSG_ERROR, (PS8)"Direction(Above=0, Below=1) crossed= %d\n", crossInfo[1]);
                break;
            }
            case IPC_EVENT_SCAN_FAILED:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_SCAN_FAILED\n");
                break;
            case IPC_EVENT_WPS_SESSION_OVERLAP:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_WPS_SESSION_OVERLAP\n");
                break;
            case IPC_EVENT_RSSI_SNR_TRIGGER:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_RSSI_SNR_TRIGGER (index = %d), Data = %d\n", (S8)(*(pData + 2) - 1),(S8)(*pData));
                break;
            case IPC_EVENT_TIMEOUT:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_TIMEOUT\n");
                break;
            case IPC_EVENT_GWSI:
                os_error_printf(CU_MSG_ERROR, (PS8)"IpcEvent_PrintEvent - received IPC_EVENT_GWSI\n");
                break;
            case IPC_EVENT_LOGGER:
#ifdef ETH_SUPPORT
                ProcessLoggerMessage(pData, (U16)DataLen);
#endif   
                break;
            default :
                os_error_printf(CU_MSG_ERROR, (PS8)"**** Unknow EventId %d ****\n", EventId); 
        }
    }   
}

static VOID IpcEvent_wext_event_wireless(IpcEvent_Child_t* pIpcEventChild, PS8 data, S32 len)
{
    struct iw_event     iwe;      
    struct stream_descr stream;                 
    S32                 ret;  
    IPC_EV_DATA*        pEvent;    
	U32					EventId = 0;

    /* init the event stream */
    os_memset((char *)&stream, '\0', sizeof(struct stream_descr));
    stream.current = (char *)data;
    stream.end = (char *)(data + len);

    do  
    {     
        /* Extract an event and print it */   
        ret = ParsEvent_GetEvent(&stream, &iwe);     

        if(ret <= 0)   
            break;

        switch (iwe.cmd) 
        {
            case SIOCGIWAP:
                if((iwe.u.ap_addr.sa_data[0] == 0) && 
                    (iwe.u.ap_addr.sa_data[1] == 0) &&
                    (iwe.u.ap_addr.sa_data[2] == 0) &&
                    (iwe.u.ap_addr.sa_data[3] == 0) &&
                    (iwe.u.ap_addr.sa_data[4] == 0) &&
                    (iwe.u.ap_addr.sa_data[5] == 0))
                {
					EventId=IPC_EVENT_DISASSOCIATED;
                     IpcEvent_PrintEvent(pIpcEventChild, EventId, NULL,0);
                } 
                else 
                {
#ifdef XCC_MODULE_INCLUDED
                    /* Send a signal to the udhcpc application to trigger the renew request */
                    system("killall -SIGUSR1 udhcpc"); 
#endif
					EventId=IPC_EVENT_ASSOCIATED;
                     IpcEvent_PrintEvent(pIpcEventChild, EventId, NULL,0);                
                }
                break;
            case IWEVMICHAELMICFAILURE:
				EventId=IPC_EVENT_MEDIA_SPECIFIC;
                IpcEvent_PrintEvent(pIpcEventChild, EventId, NULL,0);
                break;
            case IWEVCUSTOM:
				pEvent =  (IPC_EV_DATA*)iwe.u.data.pointer;
                if (pEvent)
                {
                    EventId = (U32)pEvent->EvParams.uEventType;
                    IpcEvent_PrintEvent (pIpcEventChild, EventId, pEvent->uBuffer, pEvent->uBufferSize);
                }
				break;
            case SIOCGIWSCAN:
				EventId=IPC_EVENT_SCAN_COMPLETE;
                IpcEvent_PrintEvent(pIpcEventChild, EventId, NULL,0);             
                break;
            case IWEVASSOCREQIE:
                /* NOP */
                break;
            case IWEVASSOCRESPIE:
                /* NOP */
                break;
            case IWEVPMKIDCAND:
				EventId=IPC_EVENT_MEDIA_SPECIFIC;
                IpcEvent_PrintEvent(pIpcEventChild, EventId, NULL,0);    
                break;
        }

		g_tester_send_event((U8) EventId);

    }      
    while(1);
}

static VOID IpcEvent_wext_event_rtm_newlink(IpcEvent_Child_t* pIpcEventChild, struct nlmsghdr *h, 
                                            S32 len)
{
    struct ifinfomsg *ifi;
    S32 attrlen, nlmsg_len, rta_len;
    struct rtattr * attr;

    if (len < sizeof(*ifi))
        return;

    ifi = NLMSG_DATA(h);

    /*
    if ((if_nametoindex("wlan") != ifi->ifi_index) && (if_nametoindex("wifi") != ifi->ifi_index))
    {
        os_error_printf(CU_MSG_ERROR, "ERROR - IpcEvent_wext_event_rtm_newlink - Ignore event for foreign ifindex %d",
               ifi->ifi_index);
        return;
    }
    */

    nlmsg_len = NLMSG_ALIGN(sizeof(struct ifinfomsg));

    attrlen = h->nlmsg_len - nlmsg_len;
    if (attrlen < 0)
        return;

    attr = (struct rtattr *) (((char *) ifi) + nlmsg_len);

    rta_len = RTA_ALIGN(sizeof(struct rtattr));
    while (RTA_OK(attr, attrlen)) {
        if (attr->rta_type == IFLA_WIRELESS) 
        {
            IpcEvent_wext_event_wireless(pIpcEventChild, ((PS8) attr) + rta_len,
                attr->rta_len - rta_len);
        } 
        else if (attr->rta_type == IFLA_IFNAME) 
        {
            os_error_printf(CU_MSG_WARNING, (PS8)"WARNING - IpcEvent_wext_event_rtm_newlink - unsupported rta_type = IFLA_IFNAME\n");
            
        }
        attr = RTA_NEXT(attr, attrlen);
    }
}

static VOID IpcEvent_Handle_STA_Event(IpcEvent_Child_t* pIpcEventChild)
{       
    S8 buf[512];
    S32 left;
    struct sockaddr_nl from;
    socklen_t fromlen;
    struct nlmsghdr *h;

    fromlen = sizeof(from);
    left = recvfrom(pIpcEventChild->STA_socket, buf, sizeof(buf), MSG_DONTWAIT,
            (struct sockaddr *) &from, &fromlen);
    if (left < 0) 
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Handle_STA_Event - cant recv from socket %X .\n", 
						pIpcEventChild->STA_socket);
        return;
    }

    h = (struct nlmsghdr *) buf;

    while (left >= sizeof(*h)) 
    {
        S32 len, plen;

        len = h->nlmsg_len;
        plen = len - sizeof(*h);
        if (len > left || plen < 0) {
            os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Handle_STA_Event - Malformed netlink message: len=%d left=%d plen=%d",
                   len, left, plen);
            break;
        }

        switch (h->nlmsg_type) 
        {
        case RTM_NEWLINK:
            IpcEvent_wext_event_rtm_newlink(pIpcEventChild, h, plen);
            break;      
        }

        len = NLMSG_ALIGN(len);
        left -= len;
        h = (struct nlmsghdr *) ((char *) h + len);
    }

    if (left > 0) 
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Handle_STA_Event - %d extra bytes in the end of netlink ",
               left);
        IpcEvent_Handle_STA_Event(pIpcEventChild);
    }
    
}

static S32 IpcEvent_Handle_Parent_Event(IpcEvent_Child_t* pIpcEventChild)
{
    S8 msg[IPC_EVENT_MSG_MAX_LEN];

    S32 msgLen = read(pIpcEventChild->pipe_from_parent,msg, IPC_EVENT_MSG_MAX_LEN);
    msg[msgLen] = 0;

    if(!os_strcmp(msg, (PS8)IPC_EVENT_MSG_KILL))
    {
        return TRUE;
    }
    if(!os_strcmp(msg, (PS8)IPC_EVENT_MSG_UPDATE_DEBUG_LEVEL))
    {
/*        g_debug_level= pIpcEventChild->p_shared_memory->content.debug_level;*/
        return FALSE;
    }else
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Handle_Parent_Event - unknown msgLen=%d msg=|%s| \n",msgLen,msg);  

	return FALSE;

}

static VOID IpcEvent_Child_Destroy(IpcEvent_Child_t* pIpcEventChild)
{
    if(pIpcEventChild->STA_socket)
    {
        IpcEvent_Sockets_Close(pIpcEventChild->STA_socket);
    }
    
}
    
static VOID IpcEvent_Child(IpcEvent_Child_t* pIpcEventChild)
{
    /* open the socket from the driver */
    pIpcEventChild->STA_socket = IpcEvent_Sockets_Open();
    if(pIpcEventChild->STA_socket < 0)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Create - cant open socket for communication with the driver (%d)\n",pIpcEventChild->STA_socket);
        return;
    }
    
    while(1)    
    {      
        fd_set      read_set;       /* File descriptors for select */           
        S32         ret;      
        
        FD_ZERO(&read_set);
        FD_SET(pIpcEventChild->STA_socket, &read_set);
        FD_SET(pIpcEventChild->pipe_from_parent, &read_set);

#ifndef ANDROID
        ret = select(max(pIpcEventChild->pipe_from_parent,pIpcEventChild->STA_socket) + 1, 
                    &read_set, NULL, NULL, NULL);
#else
        ret = select(pIpcEventChild->STA_socket + 1, 
                    &read_set, NULL, NULL, NULL);
#endif

        if(ret < 0) 
        {             
            os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Child - Unhandled signal - exiting...\n");    
            break;
        }      
        if(ret == 0)    {     continue; }      /* Check for interface discovery events. */      
        if(FD_ISSET(pIpcEventChild->STA_socket, &read_set))
            IpcEvent_Handle_STA_Event(pIpcEventChild);    

#ifndef ANDROID 
        if(FD_ISSET(pIpcEventChild->pipe_from_parent, &read_set))   
        {
            S32 exit = IpcEvent_Handle_Parent_Event(pIpcEventChild);
             if(exit)
                break;
        }
#endif

    }  

    IpcEvent_Child_Destroy(pIpcEventChild);
}

/* functions */
/*************/
THandle IpcEvent_Create(VOID)
{   
    IpcEvent_t* pIpcEvent = (IpcEvent_t*)os_MemoryCAlloc(sizeof(IpcEvent_t), sizeof(U8));
    if(pIpcEvent == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Create - cant allocate control block\n");
        return NULL;
    }
     
    /* create a shared memory space */
    pIpcEvent->p_shared_memory = mmap(0, sizeof(IpcEvent_Shared_Memory_t), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    if ( pIpcEvent->p_shared_memory == ((PVOID)-1)) 
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Create - cant allocate shared memory\n");
        IpcEvent_Destroy(pIpcEvent);
        return NULL;
    }

    /* create a pipe */
    pipe(pIpcEvent->p_shared_memory->pipe_fields);

    /* set the event mask to all disabled */
    pIpcEvent->p_shared_memory->event_mask = 0;

    /* Create a child process */
    pIpcEvent->child_process_id = fork();

    if (0 == pIpcEvent->child_process_id)
    {
        /******************/
        /* Child process */
        /****************/      
        IpcEvent_Child_t* pIpcEventChild = (IpcEvent_Child_t*)os_MemoryCAlloc(sizeof(IpcEvent_Child_t), sizeof(U8));        
        if(pIpcEventChild == NULL)
        {
            os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcEvent_Create - cant allocate child control block\n");
            _exit(1);
        }

        pIpcEventChild->p_shared_memory = pIpcEvent->p_shared_memory;

        pIpcEventChild->pipe_from_parent = pIpcEventChild->p_shared_memory->pipe_fields[PIPE_READ];
        close(pIpcEventChild->p_shared_memory->pipe_fields[PIPE_WRITE]);

        IpcEvent_Child(pIpcEventChild);

        os_MemoryFree(pIpcEventChild);

        _exit(0);
    }
 
    pIpcEvent->pipe_to_child = pIpcEvent->p_shared_memory->pipe_fields[PIPE_WRITE];
    close(pIpcEvent->p_shared_memory->pipe_fields[PIPE_READ]);
    
    return pIpcEvent;
}

VOID IpcEvent_Destroy(THandle hIpcEvent)
{
    IpcEvent_t* pIpcEvent = (IpcEvent_t*)hIpcEvent;

    if((pIpcEvent->p_shared_memory != ((PVOID)-1)) && (pIpcEvent->p_shared_memory))
    {       
        munmap(pIpcEvent->p_shared_memory, sizeof(IpcEvent_Shared_Memory_t));
    }

    /* kill child process */
    kill(pIpcEvent->child_process_id, SIGKILL);
    
     os_MemoryFree(pIpcEvent);

}

S32 IpcEvent_EnableEvent(THandle hIpcEvent, U32 event)
{
    IpcEvent_t* pIpcEvent = (IpcEvent_t*)hIpcEvent;
    
    if(pIpcEvent->p_shared_memory->event_mask & ((u64)1 << event))
    {
        return EOALERR_IPC_EVENT_ERROR_EVENT_ALREADY_ENABLED;
    }
    else
    {
       pIpcEvent->p_shared_memory->event_mask |= ((u64)1 << event);
    }

    return OK;
        
}

S32 IpcEvent_DisableEvent(THandle hIpcEvent, U32 event)
{
    IpcEvent_t* pIpcEvent = (IpcEvent_t*)hIpcEvent;
    
    if(!(pIpcEvent->p_shared_memory->event_mask & (1 << event)))
    {
        return EOALERR_IPC_EVENT_ERROR_EVENT_ALREADY_DISABLED;
    }
    else
    {
        pIpcEvent->p_shared_memory->event_mask &= ~(1 << event);
    }

    return OK;
}

S32 IpcEvent_UpdateDebugLevel(THandle hIpcEvent, S32 debug_level)
{
    IpcEvent_t* pIpcEvent = (IpcEvent_t*)hIpcEvent;

    pIpcEvent->p_shared_memory->content.debug_level = debug_level;
    IpcEvent_SendMessageToChild(pIpcEvent, (PS8)IPC_EVENT_MSG_UPDATE_DEBUG_LEVEL);

    return OK;
}

