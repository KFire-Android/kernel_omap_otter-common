/*
 * ipc_sta.c
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
*   MODULE:  IPC_STA.c
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
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/wireless.h>
#include "cu_osapi.h"
#include "oserr.h"
#include "STADExternalIf.h"
#include "ipc_sta.h"

/* defines */
/***********/

/* local types */
/***************/
/* Module control block */
typedef struct IpcSta_t
{
    struct iwreq    wext_req;
    ti_private_cmd_t private_cmd;
    S32 STA_socket;
    
} IpcSta_t;

/* local variables */
/*******************/

/* local fucntions */
/*******************/

/*
 * IpcSta_Sockets_Open - Open a socket.
 * Depending on the protocol present, open the right socket. The socket
 * will allow us to talk to the driver.
 */
static S32 IpcSta_Sockets_Open(VOID)
{
    static const S32 families[] = {
        AF_INET, AF_IPX, AF_APPLETALK
    };
    U32    i;
    S32    sock;

    /*
    * Now pick any (exisiting) useful socket family for generic queries
    * Note : don't open all the socket, only returns when one matches,
    * all protocols might not be valid.
    * Workaround by Jim Kaba <jkaba@sarnoff.com>
    * Note : in 2001% of the case, we will just open the inet_sock.
    * The remaining 2002% case are not fully correct...
    */

    /* Try all families we support */
    for(i = 0; i < sizeof(families)/sizeof(int); ++i)
    {
        /* Try to open the socket, if success returns it */
        sock = socket(families[i], SOCK_DGRAM, 0);
        if(sock >= 0)
            return sock;
    }

    return -1;
}

/*
 * IpcSta_Sockets_Close - Close the socket used for ioctl.
 */
static inline VOID IpcSta_Sockets_Close(S32 skfd)
{
    close(skfd);
}


/* functions */
/*************/
THandle IpcSta_Create(const PS8 device_name)
{
    IpcSta_t* pIpcSta = (IpcSta_t*)os_MemoryCAlloc(sizeof(IpcSta_t), sizeof(U8));
    if(pIpcSta == NULL)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcSta_Create - cant allocate control block\n");
        return NULL;
    }

    /* open the socket to the driver */
    pIpcSta->STA_socket = IpcSta_Sockets_Open();
    if(pIpcSta->STA_socket == -1)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcSta_Create - cant open socket for communication with the driver\n");
        return NULL;
    }

    /* set the driver name */
    os_strcpy((PS8)pIpcSta->wext_req.ifr_ifrn.ifrn_name, device_name);   

    return pIpcSta;
}

VOID IpcSta_Destroy(THandle hIpcSta)
{
    IpcSta_t* pIpcSta = (IpcSta_t*)hIpcSta;

    /* close the socket to the driver */
    IpcSta_Sockets_Close(pIpcSta->STA_socket);

    os_MemoryFree(pIpcSta);
}

S32 IPC_STA_Private_Send(THandle hIpcSta, U32 ioctl_cmd, PVOID bufIn, U32 sizeIn, 
                                                PVOID bufOut, U32 sizeOut)

{
    IpcSta_t* pIpcSta = (IpcSta_t*)hIpcSta;
    S32 res;

    pIpcSta ->private_cmd.cmd = ioctl_cmd;
    if(bufOut == NULL)
        pIpcSta ->private_cmd.flags = PRIVATE_CMD_SET_FLAG;
    else
        pIpcSta ->private_cmd.flags = PRIVATE_CMD_GET_FLAG;

    pIpcSta ->private_cmd.in_buffer = bufIn;
    pIpcSta ->private_cmd.in_buffer_len = sizeIn;
    pIpcSta ->private_cmd.out_buffer = bufOut;
    pIpcSta ->private_cmd.out_buffer_len = sizeOut;


    pIpcSta->wext_req.u.data.pointer = &pIpcSta->private_cmd;
    pIpcSta->wext_req.u.data.length = sizeof(ti_private_cmd_t);
    pIpcSta->wext_req.u.data.flags = 0;

    res = ioctl(pIpcSta->STA_socket, SIOCIWFIRSTPRIV, &pIpcSta->wext_req);     
    if(res != OK)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IPC_STA_Private_Send - error sending Wext private IOCTL to STA driver (ioctl_cmd = %x,  res = %d, errno = %d)\n", ioctl_cmd,res,errno);
        return EOALERR_IPC_STA_ERROR_SENDING_WEXT;
    }

    return OK;
}

S32 IPC_STA_Wext_Send(THandle hIpcSta, U32 wext_request_id, PVOID p_iwreq_data, U32 len)
{
    IpcSta_t* pIpcSta = (IpcSta_t*)hIpcSta;
    S32 res;

    os_memcpy(&pIpcSta->wext_req.u.data, p_iwreq_data, len);

    res = ioctl(pIpcSta->STA_socket, wext_request_id, &pIpcSta->wext_req);
    if(res != OK)
    {
        os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IPC_STA_Wext_Send - error sending Wext IOCTL to STA driver (wext_request_id = 0x%x, res = %d, errno = %d)\n",wext_request_id,res,errno);
        return EOALERR_IPC_STA_ERROR_SENDING_WEXT;
    }
    
    os_memcpy(p_iwreq_data, &pIpcSta->wext_req.u.data, len);

    return OK;
}

