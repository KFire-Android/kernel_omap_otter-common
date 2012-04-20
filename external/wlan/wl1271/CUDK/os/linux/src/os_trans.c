/*
 * os_trans.c
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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>


#include "cu_osapi.h"
#include "os_trans.h"

TI_BOOL os_trans_create()
{
    return TRUE;
}


TI_BOOL os_socket (THandle* pSock)
{
    SOCKET socket_id;
    TI_BOOL optval = TRUE;

    socket_id = socket(PF_INET, SOCK_STREAM, 0);

    if (socket_id==SOCKET_ERROR)
    {
        /* Error opening socket */
        os_error_printf(CU_MSG_ERROR, "%s: error opening socket.\n", __FUNCTION__);
        return (FALSE);
    }

    /*************************/
    /* Configure the socket */
    /***********************/

    if (setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR, (const THandle)&optval, sizeof(optval)) == OSAL_ERROR) {
        /* Error setting socket option */
        os_error_printf(CU_MSG_ERROR, "%s: error setting socket option. Error %d\n", __FUNCTION__, os_get_last_error());
        close(socket_id); //close socket
        return FALSE;
    }

    *pSock = (THandle) socket_id;

    return TRUE;
}


TI_BOOL os_bind (THandle sock, U16 port)
{
    struct sockaddr_in server_addr;
    TI_SIZE_T result;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    server_addr.sin_port = htons(port);

    result = bind((SOCKET)sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (result != OK) {
        /* Error binding socket */
        os_error_printf(CU_MSG_ERROR, "%s: error binding socket. Error %d\n", __FUNCTION__, os_get_last_error());
        close((SOCKET)sock);
        return(FALSE);
    }

    return TRUE;
}


TI_BOOL os_sockWaitForConnection (THandle socket_id, THandle* pConnSock)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    TI_SIZE_T result;

    result = listen((SOCKET)socket_id, MAX_QUEUE_LENGTH);

    if (result == OSAL_ERROR) {
        /* Error listening to socket */
        /*       os_error_printf(CU_MSG_ERROR, "%s: error listening to socket. Error %d\n", __FUNCTION__, errno);
        *pConnSock = NULL;
        closesocket((SOCKET)socket_id);
        return(FALSE);*/
    }

    /**********************/
    /* Accept connection */
    /********************/
    client_addr_len = sizeof(client_addr);

    /* We suppose to get new socket id after accept (blocking action) */
    result = accept((SOCKET)socket_id, (struct sockaddr *)&client_addr, &client_addr_len);

    if (result == OSAL_ERROR) {			
        *pConnSock = NULL;
        /* Error accepting connection */
        os_error_printf(CU_MSG_ERROR, "%s: error accepting connection. Error %d\n", __FUNCTION__, os_get_last_error());
        close((SOCKET) socket_id);
        return(FALSE);
    }

    *pConnSock = (THandle) result;

    return TRUE;
}


TI_BOOL os_sockSend (THandle socket_id, PS8 buffer, U32 bufferSize)
{
    TI_SIZE_T result;

    /* Write to the socket */
    result = send( (SOCKET) socket_id, buffer, (U32)bufferSize, 0);
    if (result == SOCKET_ERROR) {

        /**************************/
        /* Error writing to port */
        /************************/
        os_error_printf(CU_MSG_ERROR, "%s: Error writing to socket (result = %d), error %d\n", 
            __FUNCTION__, result, os_get_last_error());

        return FALSE;
    }

    return TRUE;
}

VOID os_trans_destroy()
{

}

S32 os_sockRecv (THandle socket_id, PU8 pBuffer, U32 bufferSize, TI_SIZE_T flags)

{
    TI_SIZE_T result;

    /* Read from the socket */
    result = recv((SOCKET)socket_id, pBuffer, bufferSize, flags);
    if (result == SOCKET_ERROR) {

        /***************************/
        /* Error reading from port */
        /***************************/

        os_error_printf(CU_MSG_ERROR, "%s: Error reading from socket (result = %d), error %d\n", __FUNCTION__, result, os_get_last_error());
    }

    return (S32)result;
}

