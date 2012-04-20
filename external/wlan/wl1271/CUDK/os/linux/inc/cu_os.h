/*
 * cu_os.h
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

/****************************************************************************/
/*                                                                          */
/*    MODULE:   CuWext.h                                                    */
/*    PURPOSE:                                                              */
/*                                                                          */
/****************************************************************************/
#ifndef _CU_WEXT_H_
#define _CU_WEXT_H_

#include "cu_osapi.h"
#include "osDot11.h"

/* defines */
/***********/
#define MAX_SSID_LEN    32
#define MAX_PATH        260

/* types */
/*********/

/* functions */
/*************/

S32 CuOs_GetDriverThreadId(THandle hCuWext, U32* threadid);

THandle CuOs_Create(THandle hIpcSta);
VOID CuOs_Destroy(THandle hCuWext);

S32 CuOs_Get_SSID(THandle hCuWext, OS_802_11_SSID* ssid);
S32 CuOs_Get_BSSID(THandle hCuWext, TMacAddr bssid);
S32 CuOs_GetCurrentChannel(THandle hCuWext, U32* channel);
S32 CuOs_Start_Scan(THandle hCuWext, OS_802_11_SSID* ssid, TI_UINT8 scanType);
S32 CuOs_GetBssidList(THandle hCuWext, OS_802_11_BSSID_LIST_EX *bssidList);
S32 CuOs_Set_BSSID(THandle hCuWext, TMacAddr bssid);
S32 CuOs_Set_ESSID(THandle hCuWext, OS_802_11_SSID* ssid);
S32 CuOs_GetTxPowerLevel(THandle hCuWext, S32* pTxPowerLevel);
S32 CuOs_SetTxPowerLevel(THandle hCuWext, S32 txPowerLevel);
S32 CuOs_GetRtsTh(THandle hCuWext, PS32 pRtsTh);
S32 CuOs_SetRtsTh(THandle hCuWext, S32 RtsTh);
S32 CuOs_GetFragTh(THandle hCuWext, PS32 pFragTh);
S32 CuOs_SetFragTh(THandle hCuWext, S32 FragTh);

#endif  /* _CU_WEXT_H_ */
        
