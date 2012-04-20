/*
 * ipc_event.h
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
/*    MODULE:   IPC_Event.h                                                 */
/*    PURPOSE:                                                              */
/*                                                                          */
/****************************************************************************/
#ifndef _IPC_EVENT_H_
#define _IPC_EVENT_H_

/* defines */
/***********/

/* types */
/*********/

/* functions */
/*************/
THandle IpcEvent_Create(VOID);
VOID IpcEvent_Destroy(THandle hIpcEvent);

S32 IpcEvent_EnableEvent(THandle hIpcEvent, U32 event);
S32 IpcEvent_DisableEvent(THandle hIpcEvent, U32 event);
S32 IpcEvent_UpdateDebugLevel(THandle hIpcEvent, S32 debug_level);

#endif  /* _IPC_EVENT_H_ */
        
