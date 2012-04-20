/*
 * PowerMgrKeepAlive.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
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


/** 
 * \file  PowerMgrKeepAlive.c
 * \brief implement user keep-alive messages
 */

#define __FILE_ID__  FILE_ID_73
#include "osTIType.h"
#include "TWDriver.h"
#include "STADExternalIf.h"
#include "txCtrl_Api.h"

typedef struct
{
    TI_HANDLE           hTWD;
    TI_HANDLE           hReport;
    TI_HANDLE           hOs;
    TI_HANDLE           hTxCtrl;
    TKeepAliveConfig    tCurrentConfig;
    TI_BOOL             bConnected;
    TI_UINT8            wlanHeader[ WLAN_WITH_SNAP_QOS_HEADER_MAX_SIZE + AES_AFTER_HEADER_FIELD_SIZE ];
    TI_UINT32           wlanHeaderLength;
    TI_UINT8            tempBuffer[ KEEP_ALIVE_TEMPLATE_MAX_LENGTH + WLAN_WITH_SNAP_QOS_HEADER_MAX_SIZE + AES_AFTER_HEADER_FIELD_SIZE ];
} TPowerMgrKL;

TI_STATUS powerMgrKLConfigureMessage (TI_HANDLE hPowerMgrKL, TI_UINT32 uMessageIndex);

/** 
 * \fn     powerMgrKL_create 
 * \brief  Creates the power manager keep-alive sub-module
 * 
 * Allocates memory for the keep-alive object
 * 
 * \param  hOS - handle to the os object
 * \return A handle to the power manager keep-alive sub-module 
 * \sa     powerMgrKL_destroy, powerMgrKL_init
 */
TI_HANDLE powerMgrKL_create (TI_HANDLE hOS)
{
    TPowerMgrKL     *pPowerMgrKL;

    /* allocate memory for the power manager keep-alive object */
    pPowerMgrKL = os_memoryAlloc (hOS, sizeof(TPowerMgrKL));
    if ( NULL == pPowerMgrKL)
    {
        return NULL;
    }

    /* store OS handle */
    pPowerMgrKL->hOs = hOS;

    return (TI_HANDLE)pPowerMgrKL;
}

/** 
 * \fn     powerMgrKL_destroy 
 * \brief  Destroys the power manager keep-alive sub-module
 * 
 * De-allocates keep-alive object memory
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \return None
 * \sa     powerMgrKL_create, powerMgrKL_init
 */
void powerMgrKL_destroy (TI_HANDLE hPowerMgrKL)
{
    TPowerMgrKL     *pPowerMgrKL = (TPowerMgrKL*)hPowerMgrKL;

    os_memoryFree (pPowerMgrKL->hOs, hPowerMgrKL, sizeof(TPowerMgrKL));
}

/** 
 * \fn     powerMgrKL_init 
 * \brief  Initailize the power manager keep-alive sub-module
 * 
 * Stores handles to other modules
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \param  hReport - handle to the report object
 * \param  hTWD - handle to the TWD object
 * \param  hTxCtrl - handle to the TX control object
 * \return None
 * \sa     powerMgrKL_destroy, powerMgrKL_create, powerMgrKL_setDefaults
 */
void powerMgrKL_init (TI_HANDLE hPowerMgrKL,
                      TStadHandlesList *pStadHandles)
{
    TPowerMgrKL     *pPowerMgrKL = (TPowerMgrKL*)hPowerMgrKL;

    /* store handles */
    pPowerMgrKL->hTWD       = pStadHandles->hTWD;
    pPowerMgrKL->hReport    = pStadHandles->hReport;
    pPowerMgrKL->hTxCtrl    = pStadHandles->hTxCtrl;
}

/** 
 * \fn     powerMgrKL_setDefaults
 * \brief  Set powr-manager keep-aive default initialization values
 * 
 * Set powr-manager keep-aive default initialization values
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \return None
 * \sa     powerMgrKL_init
 */
void powerMgrKL_setDefaults (TI_HANDLE hPowerMgrKL)
{
    TPowerMgrKL     *pPowerMgrKL = (TPowerMgrKL*)hPowerMgrKL;
    TI_UINT32       uIndex;

    /* mark the global enable / disable flag as enabled */
    pPowerMgrKL->tCurrentConfig.enaDisFlag = TI_TRUE;

    /* mark all messages as disabled */
    for (uIndex = 0; uIndex < KEEP_ALIVE_MAX_USER_MESSAGES; uIndex++)
    {
        pPowerMgrKL->tCurrentConfig.templates[ uIndex ].keepAliveParams.enaDisFlag = TI_FALSE;
    }

    /* mark STA as disconnected */
    pPowerMgrKL->bConnected = TI_FALSE;
}

/** 
 * \fn     powerMgrKL_start
 * \brief  Notifies the power-manager keep-alive upon connection to a BSS
 * 
 * Set all configured templates to the FW
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \return TI_OK if succesful, TI_NOK otherwise 
 * \sa     powerMgrKL_stop, powerMgrKL_setParam
 */
TI_STATUS powerMgrKL_start (TI_HANDLE hPowerMgrKL)
{
    TPowerMgrKL     *pPowerMgrKL = (TPowerMgrKL*)hPowerMgrKL;
    TI_UINT32       uIndex;
    TI_STATUS       status = TI_OK;

    /* mark STA as connected */
    pPowerMgrKL->bConnected = TI_TRUE;

    /* build WLAN header */
    status = txCtrlServ_buildWlanHeader (pPowerMgrKL->hTxCtrl, &(pPowerMgrKL->wlanHeader[ 0 ]), &(pPowerMgrKL->wlanHeaderLength));

    /* download all enabled templates to the FW (through TWD)*/
    for (uIndex = 0; uIndex < KEEP_ALIVE_MAX_USER_MESSAGES; uIndex++)
    {
        /* 
         * if the message is enabled (disabled messages shouldn't be configured on connection, 
         * as they are disabled by default in the FW) */

        if (TI_TRUE == pPowerMgrKL->tCurrentConfig.templates[ uIndex ].keepAliveParams.enaDisFlag)
        {
            /* configure keep-alive to the FW (through command builder */
            status = powerMgrKLConfigureMessage (hPowerMgrKL, uIndex);
        }
    }
    return status;
}

/** 
 * \fn     powerMgrKL_stop 
 * \brief  Notifies the power-manager keep-alive upon disconnection from a BSS
 * 
 * Delete all configured templates from the FW and internal storage
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \return TI_OK if succesful, TI_NOK otherwise 
 * \sa     powerMgrKL_start, powerMgrKL_setParam
 */
void powerMgrKL_stop (TI_HANDLE hPowerMgrKL, TI_BOOL bDisconnect)
{
    TPowerMgrKL     *pPowerMgrKL = (TPowerMgrKL*)hPowerMgrKL;
    TI_UINT32       uIndex;

    /* mark STA as disconnected */
    pPowerMgrKL->bConnected = TI_FALSE;

    /* if this is a real disconnect */
    if (TI_TRUE == bDisconnect)
    {
        /* for all congfiured messages */
        for (uIndex = 0; uIndex < KEEP_ALIVE_MAX_USER_MESSAGES; uIndex++)
        {
            /* mark the message as disabled */
            pPowerMgrKL->tCurrentConfig.templates[ uIndex ].keepAliveParams.enaDisFlag = TI_FALSE;
        }
    }
    /* for roaming, don't do anything */
}

/** 
 * \fn     powerMgrKL_setParam 
 * \brief  Handles a parametr change from user-mode
 * 
 * Handles addition / removal of a template and global enable / disable flag
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \param  pParam - A pointer to the paramter being set
 * \return TI_OK if succesful, TI_NOK otherwise 
 * \sa     powerMgrKL_start, powerMgrKL_stop
 */
TI_STATUS powerMgrKL_setParam (TI_HANDLE hPowerMgrKL, paramInfo_t *pParam)
{
    TPowerMgrKL         *pPowerMgrKL = (TPowerMgrKL*)hPowerMgrKL;
    TI_UINT32           uIndex;
    TKeepAliveTemplate  *pNewTmpl;
    TI_STATUS           status = TI_OK;

    TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_INFORMATION , "Keep-alive set param called with param type %d\n", pParam->paramType);

    switch (pParam->paramType)
    {
    /* global keep-alive enable / disable flag */
    case POWER_MGR_KEEP_ALIVE_ENA_DIS:

        pPowerMgrKL->tCurrentConfig.enaDisFlag = pParam->content.powerMgrKeepAliveEnaDis;
        return TWD_CfgKeepAliveEnaDis(pPowerMgrKL->hTWD, 
                                      (TI_UINT8)pParam->content.powerMgrKeepAliveEnaDis);
        break;

    /* keep-alive template and parameters configuration */
    case POWER_MGR_KEEP_ALIVE_ADD_REM:

        pNewTmpl = pParam->content.pPowerMgrKeepAliveTemplate;
        uIndex = pNewTmpl->keepAliveParams.index;

        /* if STA is connected */
        if (TI_TRUE == pPowerMgrKL->bConnected)
        {
            /* if keep-alive is already configured for this index */
            if (TI_TRUE == pPowerMgrKL->tCurrentConfig.templates[ uIndex ].keepAliveParams.enaDisFlag)
            {
                /* disable previous keep-alive */
                pPowerMgrKL->tCurrentConfig.templates[ uIndex ].keepAliveParams.enaDisFlag = TI_FALSE;
                status = TWD_CfgKeepAlive (pPowerMgrKL->hTWD, &(pPowerMgrKL->tCurrentConfig.templates[ uIndex ].keepAliveParams));
                if (TI_OK != status)
                {
                    TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_ERROR , "powerMgrKL_setParam: error trying to clear current template %d\n", status);
                    return status;
                }
            }

            /* copy configuration */
            os_memoryCopy (pPowerMgrKL->hOs, &(pPowerMgrKL->tCurrentConfig.templates[ uIndex ]),
                           pNewTmpl, sizeof (TKeepAliveTemplate));

            /* configure keep-alive to the FW (through command builder */
            return powerMgrKLConfigureMessage (hPowerMgrKL, uIndex);
        }
        /* STA disconnected */
        else
        {
            /* copy configuration */
            os_memoryCopy (pPowerMgrKL->hOs, &(pPowerMgrKL->tCurrentConfig.templates[ uIndex ]),
                           pNewTmpl, sizeof (TKeepAliveTemplate));
        }
        return TI_OK;
        break;

    default:
        TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_ERROR , "power manager keep-alive: set param with unknown param type %d\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
        break;
    }
}

/** 
 * \fn     powerMgrKL_getParam 
 * \brief  Handles a parametr request from user-mode
 * 
 * Retrieves configuration
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \param  pParam - A pointer to the paramter being retrieved
 * \return TI_OK if succesful, TI_NOK otherwise 
 * \sa     powerMgrKL_start, powerMgrKL_stop
 */
TI_STATUS powerMgrKL_getParam (TI_HANDLE hPowerMgrKL, paramInfo_t *pParam)
{
    TPowerMgrKL     *pPowerMgrKL = (TPowerMgrKL*)hPowerMgrKL;

    TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_INFORMATION , "Keep-alive get param called with param type %d\n", pParam->paramType);

    switch (pParam->paramType)
    {
    case POWER_MGR_KEEP_ALIVE_GET_CONFIG:

        pParam->paramLength = sizeof (TKeepAliveConfig);
        os_memoryCopy (pPowerMgrKL->hOs, (void*)pParam->content.pPowerMgrKeepAliveConfig,
                       (void*)&(pPowerMgrKL->tCurrentConfig), sizeof (TKeepAliveConfig));

        return TI_OK;
        break;

    default:
        TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_ERROR , "power manager keep-alive: get param with unknown param type %d\n", pParam->paramType);
        return PARAM_NOT_SUPPORTED;
        break;
    }
}

/** 
 * \fn     powerMgrKLConfigureMessage 
 * \brief  Configures keep-alive message (template and parameters)
 * 
 * Configures a keepa-live message from internal database.
 * 
 * \param  hPowerMgrKL - handle to the power-manager keep-alive object
 * \param  uMessageIndex - index of message to configure (from internal database)
 * \return TI_OK if succesful, TI_NOK otherwise 
 * \sa     powerMgrKL_start, powerMgrKL_setParam
 */
TI_STATUS powerMgrKLConfigureMessage (TI_HANDLE hPowerMgrKL, TI_UINT32 uMessageIndex)
{
    TPowerMgrKL     *pPowerMgrKL    = (TPowerMgrKL*)hPowerMgrKL;
    TI_STATUS       status          = TI_OK;

    /* if the keep-alive for this index is enabled */
    if (TI_TRUE == pPowerMgrKL->tCurrentConfig.templates[ uMessageIndex ].keepAliveParams.enaDisFlag)
    {
        /* configure template - first the template itself */
        TSetTemplate    tTemplate;

        tTemplate.type = KEEP_ALIVE_TEMPLATE;
        tTemplate.index = uMessageIndex;
        os_memoryCopy (pPowerMgrKL->hOs, &(pPowerMgrKL->tempBuffer),
                       &(pPowerMgrKL->wlanHeader), pPowerMgrKL->wlanHeaderLength); /* copy WLAN header - was built on connection */
        os_memoryCopy (pPowerMgrKL->hOs, &(pPowerMgrKL->tempBuffer[ pPowerMgrKL->wlanHeaderLength ]),
                       &(pPowerMgrKL->tCurrentConfig.templates[ uMessageIndex ].msgBuffer[ 0 ]),
                       pPowerMgrKL->tCurrentConfig.templates[ uMessageIndex ].msgBufferLength); /* copy template data */
        tTemplate.ptr = &(pPowerMgrKL->tempBuffer[ 0 ]);                                                                     
        tTemplate.len = pPowerMgrKL->tCurrentConfig.templates[ uMessageIndex ].msgBufferLength + pPowerMgrKL->wlanHeaderLength;
        tTemplate.uRateMask = RATE_MASK_UNSPECIFIED;
        status = TWD_CmdTemplate (pPowerMgrKL->hTWD, &tTemplate, NULL, NULL);
        if (TI_OK != status)
        {
            TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_ERROR , "powerMgrKLConfigureMessage: error trying to set new template %d\n", status);
            return status;
        }

        /* and than the parameters */
        status = TWD_CfgKeepAlive (pPowerMgrKL->hTWD, &(pPowerMgrKL->tCurrentConfig.templates[ uMessageIndex ].keepAliveParams));
        if (TI_OK != status)
        {
            TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_ERROR , "powerMgrKLConfigureMessage: error trying to set new keep-alive params %d\n", status);
            return status;
        }
    }
    /* keep-alive for this index is disabled - just disable it */
    else
    {
        status = TWD_CfgKeepAlive (pPowerMgrKL->hTWD, &(pPowerMgrKL->tCurrentConfig.templates[ uMessageIndex ].keepAliveParams));
        if (TI_OK != status)
        {
            TRACE1(pPowerMgrKL->hReport, REPORT_SEVERITY_ERROR , "powerMgrKLConfigureMessage: error trying to set new keep-alive params %d\n", status);
            return status;
        }
    }

    return status;
}
