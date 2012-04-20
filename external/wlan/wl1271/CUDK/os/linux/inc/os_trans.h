/*
 * os_trans.h
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
#ifndef _OS_TRANS_H
#define _OS_TRANS_H

#include "cu_osapi.h"
#include "tidef.h"

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#define MAX_QUEUE_LENGTH	5//maximum length the queue of pending connections

typedef  int		SOCKET;

TI_BOOL os_trans_create(VOID);
TI_BOOL os_socket (THandle* pSock);
TI_BOOL os_bind (THandle sock, U16 port);
TI_BOOL os_sockWaitForConnection (THandle socket_id, THandle* pConnSock);
TI_BOOL os_sockSend (THandle socket_id, PS8 buffer, U32 bufferSize);
VOID	os_trans_destroy(VOID);
S32		os_sockRecv (THandle socket_id, PU8 pBuffer, U32 bufferSize, TI_SIZE_T flags);

#endif
