/*
 * ipc_wpa.c
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
*   MODULE:  Ipc_Wpa.c
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


#include "cu_osapi.h"
#include "oserr.h"
#include "wpa_ctrl.h"
#include "ipc_wpa.h"

/* defines */
/***********/
#define IPC_WPA_CTRL_OPEN_RETRIES 5

/* local types */
/***************/
/* Module control block */
typedef struct TIpcWpa
{
	struct wpa_ctrl *pWpaCtrl;
#if 0
	S32 socket;
	struct sockaddr_in local;
	struct sockaddr_in dest;
#endif
} TIpcWpa;

/* local variables */
/*******************/

/* local fucntions */
/*******************/
static S32 IpcWpa_Sockets_Open(TIpcWpa* pIpcWpa, PS8 pSupplIfFile)
{
#ifdef WPA_SUPPLICANT
	S32 i;

	for(i=0; i< IPC_WPA_CTRL_OPEN_RETRIES; i++)
	{
		pIpcWpa->pWpaCtrl = wpa_ctrl_open((char*)pSupplIfFile);
		if(pIpcWpa->pWpaCtrl)
			break;
	}
#else
	pIpcWpa->pWpaCtrl = NULL;
#endif
	if(pIpcWpa->pWpaCtrl == NULL)
	{
		os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcWpa_Sockets_Open - can't connect the socket\n");
		return EOALERR_IPC_WPA_ERROR_CANT_CONNECT_TO_SUPPL;
	}

	return OK;
}

static VOID IpcWpa_Sockets_Close(TIpcWpa* pIpcWpa)
{
#ifdef WPA_SUPPLICANT
	wpa_ctrl_close(pIpcWpa->pWpaCtrl);
#endif
}


/* functions */
/*************/
THandle IpcWpa_Create(PS32 pRes, PS8 pSupplIfFile)
{	
	TIpcWpa* pIpcWpa = (TIpcWpa*)os_MemoryCAlloc(sizeof(TIpcWpa), sizeof(U8));
	if(pIpcWpa == NULL)
	{
		*pRes = OK;
		os_error_printf(CU_MSG_ERROR, (PS8)"ERROR - IpcWpa_Create - cant allocate control block\n");
		return NULL;
	}

	*pRes = IpcWpa_Sockets_Open(pIpcWpa, pSupplIfFile);
	if(*pRes)
	{
		IpcWpa_Destroy(pIpcWpa);
		return NULL;
	}

	return pIpcWpa;
}

VOID IpcWpa_Destroy(THandle hIpcWpa)
{
	TIpcWpa* pIpcWpa = (TIpcWpa*)hIpcWpa;

	if(pIpcWpa->pWpaCtrl)
		IpcWpa_Sockets_Close(pIpcWpa);

	os_MemoryFree(pIpcWpa);
}

S32 IpcWpa_Command(THandle hIpcWpa, PS8 cmd, S32 print)
{
#ifdef WPA_SUPPLICANT
	TIpcWpa* pIpcWpa = (TIpcWpa*)hIpcWpa;
	S8  Resp[IPC_WPA_RESP_MAX_LEN];
	TI_SIZE_T RespLen = IPC_WPA_RESP_MAX_LEN - 1;
	S32 ret;

	ret = wpa_ctrl_request(pIpcWpa->pWpaCtrl, (char*)cmd, os_strlen(cmd), (char*)Resp, (size_t*)&RespLen, NULL);

	if (ret == -2)
	{
		os_error_printf(CU_MSG_ERROR, (PS8)"'%s' command timed out.\n", cmd);
		return EOALERR_IPC_WPA_ERROR_CMD_TIMEOUT;
	}
	else if (ret < 0)
	{
		os_error_printf(CU_MSG_ERROR, (PS8)"'%s' command failed (%d).\n", cmd, ret);
		return EOALERR_IPC_WPA_ERROR_CMD_FAILED;
	}
	if (print)
	{
		Resp[RespLen] = '\0';
		os_error_printf(CU_MSG_INFO2, (PS8)"%s", Resp);
	}
	return OK;
#else
	return EOALERR_IPC_WPA_ERROR_CMD_FAILED;
#endif
}

S32 IpcWpa_CommandWithResp(THandle hIpcWpa, PS8 cmd, S32 print, PS8 pResp, PU32 pRespLen)
{
#ifdef WPA_SUPPLICANT
	TIpcWpa* pIpcWpa = (TIpcWpa*)hIpcWpa;
	S32 ret;

	*pRespLen = IPC_WPA_RESP_MAX_LEN - 1;
	ret = wpa_ctrl_request(pIpcWpa->pWpaCtrl, (char*)cmd, os_strlen(cmd), (char*)pResp, (size_t*)pRespLen, NULL);

	if (ret == -2)
	{
		os_error_printf(CU_MSG_ERROR, (PS8)"'%s' command timed out.\n", cmd);
		return EOALERR_IPC_WPA_ERROR_CMD_TIMEOUT;
	}
	else if (ret < 0)
	{
		os_error_printf(CU_MSG_ERROR, (PS8)"'%s' command failed.\n", cmd);
		return EOALERR_IPC_WPA_ERROR_CMD_FAILED;
	}
	if (print)
	{
		pResp[*pRespLen] = '\0';
		os_error_printf(CU_MSG_INFO2, (PS8)"%s", pResp);
	}
	return OK;
#else
	return EOALERR_IPC_WPA_ERROR_CMD_FAILED;
#endif
}
