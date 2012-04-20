/*
 * ParsEvent.c
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
*   MODULE:  ParsEvent.c
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
#include <stdint.h>
#include <sys/types.h>
#ifdef ANDROID
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <linux/wireless.h>
#include "ParsEvent.h"
#include "cu_osapi.h"

#define IW_DESCR_FLAG_NONE      0x0000  
#define IW_DESCR_FLAG_DUMP      0x0001  
#define IW_DESCR_FLAG_EVENT     0x0002  
#define IW_DESCR_FLAG_RESTRICT  0x0004  
#define IW_DESCR_FLAG_NOMAX     0x0008  
#define IW_DESCR_FLAG_WAIT      0x0100  

/* local types */
/***************/
static int ParsEvent_GetEventParam( unsigned short  uEventCmd, 
                                        unsigned int*   uEventLen, 
                                        unsigned int*   pEventFlag,
                                        unsigned short* pMaxPayload,
                                        unsigned short* pPayloadNum,
                                        unsigned int*   bIsPoint);
/** 
 * \fn     ParsEvent_GetEvent()
 * \brief  get next event from the event stream.
 * support the following events that CLI uses: SIOCGIWAP, SIOCGIWESSID, SIOCGIWNAME, SIOCGIWMODE, 
 *                                             SIOCGIWFREQ, IWEVQUAL, SIOCGIWENCODE, SIOCGIWRATE, IWEVCUSTOM, 
 *                                             IWEVMICHAELMICFAILURE, SIOCGIWSCAN, IWEVASSOCREQIE, IWEVASSOCRESPIE, 
 *                                             IWEVPMKIDCAND.
 * \note   
 * \param  pEventStream - Event stream pointer.
 * \param  pEvent - return Event.
 * \return value > 0 meens valide event 
 * \sa     
 */ 
intParsEvent_GetEvent(struct stream_descr* pEventStream, struct iw_event* pEvent)

{
    unsigned int uStatus = 0;
    char*        pPtr;
    unsigned int uEventLen = 1;
    unsigned int uDataLen = 0;
    unsigned int   uEventFlag = 0;
    unsigned short uMaxPayload = 0;
    unsigned short uPayloadNum = 0;
    unsigned int   uMoreLen;
    unsigned int   bIsPoint = 0;

    /* Check validity */
    if((pEventStream->current + IW_EV_LCP_LEN) > pEventStream->end)
    {
        return 0;
    }
    
    /* copy tha event */
    os_memcpy((char *)pEvent, pEventStream->current, IW_EV_LCP_LEN);
        
    /* Check validity */
    if(pEvent->len <= IW_EV_LCP_LEN)
    {
        return 0;
    }

    /* get event parameters */
    uStatus = ParsEvent_GetEventParam(pEvent->cmd, &uEventLen, &uEventFlag, &uMaxPayload, &uPayloadNum, &bIsPoint);

    if(uEventLen <= IW_EV_LCP_LEN)
    {
        /* jump to next event */
        pEventStream->current += pEvent->len;
        return 1;
    }

    /* get payload */
    if(pEventStream->value == NULL)
    {
        pPtr = pEventStream->current + IW_EV_LCP_LEN; 
    }
    else
    {
        pPtr = pEventStream->value;          
    }
        
    uDataLen = uEventLen - IW_EV_LCP_LEN;

    /* Check validity */
    if((pPtr + uDataLen) > pEventStream->end)
    {
        /* jump to next event */
        pEventStream->current += pEvent->len;
        return 0;
    }

    if(bIsPoint == TRUE)
    {
        os_memcpy((char *) pEvent + IW_EV_LCP_LEN + IW_EV_POINT_OFF,pPtr, uDataLen);
    }
    else
    {
        os_memcpy((char *) pEvent + IW_EV_LCP_LEN, pPtr, uDataLen);
    }
    
    /* jump to next event */
    pPtr = pPtr + uDataLen;
    
    if(bIsPoint == FALSE)
    {
        /* verify more values */
        if((pPtr + uDataLen) > (pEventStream->current + pEvent->len))
        {
            pEventStream->current += pEvent->len;
            pEventStream->value = NULL;
        }
        else
        {
            pEventStream->value = pPtr;
        }

    }
    else
    {
        uMoreLen = pEvent->len - uEventLen;

        if((uMoreLen == 0) ||
           ( uStatus == 0) ||
           (!(uEventFlag & IW_DESCR_FLAG_NOMAX) && (pEvent->u.data.length > uMaxPayload)) ||
           ((pEvent->u.data.length * uPayloadNum) > uMoreLen)
          )
        {
            pEvent->u.data.pointer = NULL;
        }
        else
        {
            pEvent->u.data.pointer = pPtr;
        }

        pEventStream->current += pEvent->len;
    }

    return 1;
}


static int ParsEvent_GetEventParam( unsigned short  uEventCmd, 
                                    unsigned int*   uEventLen, 
                                    unsigned int*   pEventFlag,
                                    unsigned short* pMaxPayload,
                                    unsigned short* pPayloadNum,
                                    unsigned int*   bIsPoint)
{

    switch(uEventCmd)
    {
    case SIOCGIWAP:
        *uEventLen = IW_EV_ADDR_LEN;
        *pEventFlag = IW_DESCR_FLAG_DUMP;
        *bIsPoint = FALSE;
    break;

    case SIOCGIWESSID:
        *uEventLen = IW_EV_POINT_LEN;
        *pEventFlag = IW_DESCR_FLAG_DUMP;
        *pMaxPayload = IW_ESSID_MAX_SIZE + 1;
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;

    case SIOCGIWNAME:
        *uEventLen = IW_EV_CHAR_LEN;
        *pEventFlag = IW_DESCR_FLAG_DUMP;
        *bIsPoint = FALSE;
    break;
    case SIOCGIWMODE:
        *uEventLen = IW_EV_UINT_LEN;
        *pEventFlag = IW_DESCR_FLAG_DUMP;
        *bIsPoint = FALSE;
    break;
    case SIOCGIWFREQ:
        *uEventLen = IW_EV_FREQ_LEN;
        *pEventFlag = IW_DESCR_FLAG_DUMP;
        *bIsPoint = FALSE;
    break;
    case IWEVQUAL:
        *uEventLen = IW_EV_QUAL_LEN;
        *bIsPoint = FALSE;
    break;
    case SIOCGIWENCODE:
        *uEventLen = IW_EV_POINT_LEN;
        *pEventFlag = IW_DESCR_FLAG_DUMP | IW_DESCR_FLAG_RESTRICT;
        *pMaxPayload = IW_ENCODING_TOKEN_MAX;
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;
    case SIOCGIWRATE:
        *uEventLen = IW_EV_PARAM_LEN;
        *bIsPoint = FALSE;
    break;
    case IWEVCUSTOM:
        *uEventLen = IW_EV_POINT_LEN;
        *pMaxPayload = IW_CUSTOM_MAX;
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;
    case IWEVMICHAELMICFAILURE:
        *uEventLen = IW_EV_POINT_LEN;
        *pMaxPayload = sizeof(struct iw_michaelmicfailure);
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;
    case SIOCGIWSCAN:
        *uEventLen = IW_EV_POINT_LEN;
        *pEventFlag = IW_DESCR_FLAG_NOMAX;
        *pMaxPayload = IW_SCAN_MAX_DATA;
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;
    case IWEVASSOCREQIE:
        *uEventLen = IW_EV_POINT_LEN;
        *pMaxPayload = IW_GENERIC_IE_MAX;
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;
    case IWEVASSOCRESPIE    :
        *uEventLen = IW_EV_POINT_LEN;
        *pMaxPayload = IW_GENERIC_IE_MAX;
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;
    case IWEVPMKIDCAND:
        *uEventLen = IW_EV_POINT_LEN;
        *pMaxPayload = sizeof(struct iw_pmkid_cand);
        *pPayloadNum = 1;
        *bIsPoint = TRUE;
    break;
    default:
         return 0;
    }

    return 1;
}

