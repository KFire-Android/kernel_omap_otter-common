/*
 * EvHandler.c
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
 * All rights reserved.                                                  
 *                                                                       
 * Redistribution and use in source and binary forms, with or without    
 * modification, are permitted provided that the following conditions    
 * are met:                                                              
 *                                                                       
 *  * Redistributions of source code must retain the above copyright     
 *    notice, this list of conditions and the following disclaimer.      
 *  * Redistributions in binary form must reproduce the above copyright  
 *    notice, this list of conditions and the following disclaimer in    
 *    the documentation and/or other materials provided with the         
 *    distribution.                                                      
 *  * Neither the name Texas Instruments nor the names of its            
 *    contributors may be used to endorse or promote products derived    
 *    from this software without specific prior written permission.      
 *                                                                       
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS   
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT     
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT      
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT   
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define __FILE_ID__  FILE_ID_50
#include "IPCKernelApi.h"
#include "EvHandler.h"
#include "osApi.h"
#include "osDebug.h"

#ifndef _WINDOWS
#include "windows_types.h"
#else
#include <windows.h>
#endif /*_WINDOWS*/

#ifdef EV_HANDLER_DEBUG
TI_HANDLE ghEvHandler; /* for debug, remove later*/
#endif

/* ************************** Upper Interface **********************************/

TI_HANDLE EvHandler_Create (TI_HANDLE hOs)
{
    TEvHandlerObj *pEvHandler;

    PRINT(DBG_INIT_LOUD, (" EvHandlerInit\n"));
    pEvHandler = os_memoryAlloc(hOs,sizeof(TEvHandlerObj));

    if(NULL == pEvHandler)
    {
        PRINT(DBG_INIT_LOUD, ("EvHandler_Create() - Allocation failed! Returning NULL.\n"));
        return NULL;
    }

    os_memoryZero(hOs,pEvHandler,sizeof(TEvHandlerObj));

    #ifdef EV_HANDLER_DEBUG
    ghEvHandler= pEvHandler;
      PRINTF(DBG_INIT_VERY_LOUD, ("EvHandlerInit: ghEvHandler set to %08X\n", ghEvHandler));
    #endif

    pEvHandler->hOs = hOs;

    pEvHandler->LastUMEventType = 0xFFFFFFFF;

    return (TI_HANDLE) pEvHandler;
}

TI_UINT32 EvHandlerUnload (TI_HANDLE hEvHandler)
{

    TEvHandlerObj *pEvHandler;

    PRINT(DBG_INIT_LOUD, (" ev_handler_unLoad\n"));
    pEvHandler = (TEvHandlerObj *)hEvHandler;

    os_memoryFree(pEvHandler->hOs,pEvHandler,sizeof(TEvHandlerObj));

    return TI_OK;
}


TI_UINT32 EvHandlerRegisterEvent(TI_HANDLE hEvHandler, TI_UINT8* pData, TI_UINT32 Length)
{
    TEvHandlerObj *pEvHandler;
    IPC_EVENT_PARAMS *pEvParams;
    TI_UINT32 ModuleIndex;

    if( (hEvHandler==NULL) || (pData == NULL)){
        PRINT(DBG_INIT_ERROR, "EvHandler:EvHandlerRegisterEvent Bad Handle passed \n");
        return TI_NOK;
    }

#ifdef EV_HANDLER_DEBUG
    if (ghEvHandler !=  hEvHandler)
    {
        return TI_NOK;
    }
#endif

    pEvHandler  = (TEvHandlerObj *)hEvHandler;
    pEvParams = (IPC_EVENT_PARAMS *)pData;

    PRINTF(DBG_INIT_LOUD, (" EvHandlerRegisterEvent EventType = %d \n",pEvParams->uEventType));

    /* used to be: if ( sizeof(IPC_EVENT_PARAMS) != Length)
       relaxed size checking (okay if output buffer is larger)  */
    if (sizeof(IPC_EVENT_PARAMS) > Length)
    {
        PRINTF(DBG_INIT_ERROR, (" EvHandlerRegisterEvent Error sizeof(IPC_EVENT_PARAMS) != Length,"
                               "%d != %d \n",sizeof(IPC_EVENT_PARAMS), (int)Length));
        return (TI_UINT32)STATUS_INVALID_PARAMETER;
    }

    if (pEvParams->uEventType >= IPC_EVENT_MAX){
        PRINTF(DBG_INIT_ERROR, (" EvHandlerRegisterEvent Error - Invalid Event Type = %d \n",
              pEvParams->uEventType));
        return (TI_UINT32)STATUS_INVALID_PARAMETER;
    }

    ModuleIndex = 0;

    while ((ModuleIndex < MAX_REGISTERED_MODULES) &&
       (pEvHandler->RegistrationArray[pEvParams->uEventType][ModuleIndex].uEventID != NULL))
    {
        ModuleIndex++;
    }

    if (ModuleIndex == MAX_REGISTERED_MODULES)
    {
        PRINTF(DBG_INIT_WARNING, (" EvHandlerRegisterEvent %d "
                                  "Registration queue full or event already registered!\n",
                                  pEvParams->uEventType));
        return (TI_UINT32)STATUS_INVALID_PARAMETER;
    }

    os_memoryCopy(pEvHandler->hOs,(TI_UINT8*)&pEvHandler->RegistrationArray[pEvParams->uEventType][ModuleIndex],
                    (TI_UINT8*)pEvParams,Length);

    pEvParams->uEventID = (TI_HANDLE)&pEvHandler->RegistrationArray[pEvParams->uEventType][ModuleIndex];

    pEvHandler->RegistrationArray[pEvParams->uEventType][ModuleIndex].uEventID = pEvParams->uEventID;

    PRINT(DBG_INIT_LOUD, " EvHandlerRegisterEvent Out \n");
    return STATUS_SUCCESS;

}


TI_UINT32 EvHandlerUnRegisterEvent(TI_HANDLE hEvHandler, TI_HANDLE uEventID)
{
    TEvHandlerObj *pEvHandler;
    IPC_EVENT_PARAMS *pEvParams;
    TI_UINT32 ModuleIndex;

#ifdef EV_HANDLER_DEBUG
    if (ghEvHandler !=  hEvHandler )
    {
          return TI_NOK;
    }
#endif

    if (uEventID == NULL)
    {
        return TI_NOK;
    }

    pEvHandler = (TEvHandlerObj *)hEvHandler;
    pEvParams  = (IPC_EVENT_PARAMS *)uEventID;

    PRINTF(DBG_INIT_LOUD, (" EvHandlerUnRegisterEvent EventType = %d \n",pEvParams->uEventType));

    if( pEvParams->uEventType >= IPC_EVENT_MAX){
        PRINTF(DBG_INIT_ERROR, (" EvHandlerRegisterEvent Error Event Type = %d \n",
              pEvParams->uEventType));
        return (TI_UINT32)STATUS_INVALID_PARAMETER;
    }

    ModuleIndex = 0;

    while ((ModuleIndex < MAX_REGISTERED_MODULES) &&
           (pEvHandler->RegistrationArray[pEvParams->uEventType][ModuleIndex].uEventID != pEvParams->uEventID))
    {
        ModuleIndex++;
    }

    if (ModuleIndex == MAX_REGISTERED_MODULES)
    {
        PRINTF(DBG_INIT_ERROR, (" EvHandlerUnRegisterEvent %d "
                               "Registration queue doesn't hold this event!\n",
                               pEvParams->uEventType ));
        return (TI_UINT32)STATUS_INVALID_PARAMETER;
    }
    pEvHandler->RegistrationArray[pEvParams->uEventType][ModuleIndex].uEventID = NULL;

    return STATUS_SUCCESS;
}

/* ************************** Upper Interface End*********************************/

/* ************************** Bottom Interface **********************************/

TI_UINT32 EvHandlerSendEvent(TI_HANDLE hEvHandler, TI_UINT32 EvType, TI_UINT8* pData, TI_UINT32 Length)
{
    TEvHandlerObj *pEvHandler;
    IPC_EV_DATA*         pNewEvent;
    TI_UINT32            TailIndex=0;
    TI_UINT32            ModuleIndex=0;

    PRINTF(DBG_INIT_LOUD, (" EvHandlerSendEvent %d  \n", EvType));

    if(hEvHandler == NULL){
        PRINT(DBG_INIT_ERROR, "EvHandlerSendEvent Bad Handle passed \n");
        return TI_NOK;
    }

#ifdef EV_HANDLER_DEBUG
    if (ghEvHandler !=  hEvHandler)
    {
        return TI_NOK;
    }
#endif

    pEvHandler  = (TEvHandlerObj *)hEvHandler;

    TailIndex   = pEvHandler->SendEventArray.TailIndex;

    while ((ModuleIndex < MAX_REGISTERED_MODULES) && (EvType <= IPC_EVENT_MAX))
    {
        if (pEvHandler->RegistrationArray[EvType][ModuleIndex].uEventID != NULL )
        {
            if(pEvHandler->SendEventArray.Counter == MAX_SEND_EVENTS)
            {
                PRINT(DBG_INIT_ERROR, " EvHandlerSendEvent Array Full u Fool! \n");
                return TI_NOK;
            }

            pNewEvent = &pEvHandler->SendEventArray.Array[TailIndex];

            /* copy the event parameters and data to the events queue*/
            os_memoryCopy(pEvHandler->hOs,(TI_UINT8*)&pNewEvent->EvParams,
                            (TI_UINT8*)&pEvHandler->RegistrationArray[EvType][ModuleIndex],
                            sizeof(IPC_EVENT_PARAMS));

            os_memoryZero(pEvHandler->hOs,(TI_UINT8*)pNewEvent->uBuffer, sizeof(pNewEvent->uBuffer));

            os_memoryCopy(pEvHandler->hOs,
                          (TI_UINT8*)pNewEvent->uBuffer,
                          (TI_UINT8*)pData,
                          Length);

            pNewEvent->uBufferSize = Length;

            if(pNewEvent->EvParams.uDeliveryType ==  DELIVERY_PUSH)
            {
                    PRINTF(DBG_INIT_LOUD, (" EvHandlerSendEvent %d to OS \n", EvType));
                    PRINTF(DBG_INIT_LOUD, ("EvHandlerSendEvent Matching OS Registered event found at EvType = %d,"
                                          "ModuleIndex = %d  \n", EvType, ModuleIndex));
                    IPC_EventSend (pEvHandler->hOs,(TI_UINT8*)pNewEvent,sizeof(IPC_EV_DATA));
            }
            else {

                pEvHandler->LastUMEventType = EvType;
                pEvHandler->SendEventArray.TailIndex = (TailIndex+1) % MAX_SEND_EVENTS;
                pEvHandler->SendEventArray.Counter++;
                TailIndex   = pEvHandler->SendEventArray.TailIndex;
                PRINTF(DBG_INIT_LOUD, (" EvHandlerSendEvent %d to User Mode \n", EvType));
                PRINTF(DBG_INIT_LOUD, ("EvHandlerSendEvent Matching User Mode Registered event found at EvType = %d,"
                                       "ModuleIndex = %d  \n", EvType, ModuleIndex));
                if (pEvHandler->SendEventArray.Counter == 1)
                {
                    IPC_EventSend (pEvHandler->hOs,NULL,0);
                }
            }
        } /* end if*/

       ModuleIndex++;

   } /* end of while*/

    return TI_OK;
}

 /* ************************** Bottom Interface End **********************************/
