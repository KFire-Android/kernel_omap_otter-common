/*
 * ipc_wpa.h
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
/*    MODULE:   Ipc_Wpa.h                                                   */
/*    PURPOSE:                                                              */
/*                                                                          */
/****************************************************************************/
#ifndef _IPC_WPA_H_
#define _IPC_WPA_H_

/* defines */
/***********/
#define IPC_WPA_RESP_MAX_LEN 256

/* types */
/*********/

/* functions */
/*************/
THandle IpcWpa_Create(PS32 pRes, PS8 pSupplIfFile);
VOID IpcWpa_Destroy(THandle hIpcWpa);

S32 IpcWpa_Command(THandle hIpcWpa, PS8 cmd, S32 print);
S32 IpcWpa_CommandWithResp(THandle hIpcWpa, PS8 cmd, S32 print, PS8 pResp, PU32 pRespLen);


#endif  /* _IPC_WPA_H_ */
        
