/*
 * scr.c
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

/** \file  scr.c
 *  \brief This file include the SCR module implementation
 *
 *  \see   scr.h, scrApi.h
 */

#define __FILE_ID__  FILE_ID_82
#include "tidef.h"
#include "report.h"
#include "osApi.h"
#include "scr.h"
#include "DrvMainModules.h"


/*
 ***********************************************************************
 *  External data definitions.
 ***********************************************************************
 */

/**
 * \brief This array holds configuration values for abort others field for different clients.\n
 */
static EScrClientId abortOthers[ SCR_RESOURCE_NUM_OF_RESOURCES ][ SCR_CID_NUM_OF_CLIENTS ] = 
/* APP_SCAN           DRV_FG             CONT_SCAN          XCC_MSR            BASIC_MSR          CONNECT                IMMED_SCN              SWITCH_CHNL*/
{/* Serving channel resource */
 { SCR_CID_NO_CLIENT, SCR_CID_NO_CLIENT, SCR_CID_NO_CLIENT, SCR_CID_NO_CLIENT, SCR_CID_NO_CLIENT, SCR_CID_BASIC_MEASURE, SCR_CID_BASIC_MEASURE, SCR_CID_BASIC_MEASURE },
 /* periodic scan resource */
 { SCR_CID_NO_CLIENT, SCR_CID_APP_SCAN,  SCR_CID_NO_CLIENT, SCR_CID_NO_CLIENT, SCR_CID_NO_CLIENT, SCR_CID_DRIVER_FG_SCAN,SCR_CID_NO_CLIENT,     SCR_CID_NO_CLIENT }
};

/**
 * \brief This array holds configuration values for the client status field for different clients and groups. \n
 */
static TI_BOOL clientStatus[ SCR_RESOURCE_NUM_OF_RESOURCES ][ SCR_MID_NUM_OF_MODES ][ SCR_GID_NUM_OF_GROUPS ][ SCR_CID_NUM_OF_CLIENTS ] = 
            { 
                {/* serving channel resource */
                    {/* This is the table for Normal mode  */
                        /* client status for idle group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for DRV scan group */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,     /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for APP scan group */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for connect group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },                       
                       /* client status for connected group */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_CONT_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_XCC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for roaming group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_TRUE,     /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ }
                    },

                    /* This is the table for the Soft gemini mode   */

                    { /* client status for idle group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for DRV scan group */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for APP scan group */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for connect group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },                       
                       /* client status for connected group */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_CONT_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_XCC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for roaming group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_TRUE,     /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ }
                    }
                },
                {/* periodic-scan resource */
                    {/* This is the table for Normal mode */
                        /* client status for idle group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for DRV scan gorup */
                        { TI_FALSE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for APP scan gorup */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for connect group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },                       
                       /* client status for connected group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for roaming group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ }
                    },

                    /* This is the table for the Soft gemini mode   */

                    { /* client status for idle group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for DRV scan gorup */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_TRUE,     /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for APP scan gorup */
                        { TI_TRUE,     /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for connect group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,     /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },                       
                       /* client status for connected group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ },
                       /* client status for roaming group */
                        { TI_FALSE,    /**< client status for SCR_CID_APP_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_DRIVER_FG_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_CONT_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_XCC_MEASURE */
                          TI_FALSE,    /**< client status for SCR_CID_BASIC_MEASURE */
                          TI_TRUE,     /**< client status for SCR_CID_CONNECT */
                          TI_FALSE,    /**< client status for SCR_CID_IMMED_SCAN */
                          TI_FALSE,    /**< client status for SCR_CID_SWITCH_CHANNEL */ }
                    }
                }                
            };

                
/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Creates the SCR object
 *
 * Function Scope \e Public.\n
 * \param hOS - handle to the OS object.\n
 * \return a handle to the SCR object.\n
 */
TI_HANDLE scr_create( TI_HANDLE hOS )
{
    /* allocate the SCR object */
    TScr    *pScr = os_memoryAlloc( hOS, sizeof(TScr));

    if ( NULL == pScr )
    {
        WLAN_OS_REPORT( ("ERROR: Failed to create SCR module"));
        return NULL;
    }

    /* store the OS handle */
    pScr->hOS = hOS;

    return pScr;
}

/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Finalizes the SCR object (freeing memory)
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 */
void scr_release( TI_HANDLE hScr )
{
    TScr    *pScr = (TScr*)hScr;

    os_memoryFree( pScr->hOS, hScr, sizeof(TScr));
}

/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Initializes the SCR object
 *
 * \param  pStadHandles  - The driver modules handles
 * \return void  
 */
void scr_init (TStadHandlesList *pStadHandles)
{
    TScr        *pScr = (TScr*)(pStadHandles->hSCR);
    TI_UINT32   i, j;

    /* store the report object */
    pScr->hReport = pStadHandles->hReport;

    /* mark current group as idle */
    pScr->currentGroup = SCR_GID_IDLE;

    /* mark current mode as normal */
    pScr->currentMode = SCR_MID_NORMAL;

    /* signal not within request process */
    pScr->statusNotficationPending = TI_FALSE;

    /* mark that no client is currently running */
    for (i = 0; i < SCR_RESOURCE_NUM_OF_RESOURCES; i++)
    {
        pScr->runningClient[ i ] = SCR_CID_NO_CLIENT;
    }

    /* initialize client array */
    for (i = 0; i < SCR_CID_NUM_OF_CLIENTS; i++ )
    {
        for (j = 0; j < SCR_RESOURCE_NUM_OF_RESOURCES; j++)
        {
            pScr->clientArray[ i ].state[ j ] = SCR_CS_IDLE;
            pScr->clientArray[ i ].currentPendingReason[ j ] = SCR_PR_NONE;
        }
        pScr->clientArray[ i ].clientRequestCB = NULL;
        pScr->clientArray[ i ].ClientRequestCBObj = NULL;
    }
    
    TRACE0(pScr->hReport, REPORT_SEVERITY_INIT , ".....SCR configured successfully\n");
}

/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Registers the callback function to be used per client.
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param client - the client ID.\n
 * \param callbackFunc - the address of the callback function to use.\n
 * \param callbackObj - the handle of the object to pass to the callback function (the client object).\n
 */
void scr_registerClientCB( TI_HANDLE hScr, 
                           EScrClientId client,
                           TScrCB callbackFunc, 
                           TI_HANDLE callbackObj )
{
    TScr    *pScr = (TScr*)hScr;

#ifdef TI_DBG
    if (client >= SCR_CID_NUM_OF_CLIENTS)
    {
        TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Attempting to register callback for invalid client %d\n", client);
        return;
    }
#endif
    pScr->clientArray[ client ].clientRequestCB = callbackFunc;
    pScr->clientArray[ client ].ClientRequestCBObj = callbackObj;
}

/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Notifies the running process upon a firmware reset.
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 */
void scr_notifyFWReset( TI_HANDLE hScr )
{
    TScr        *pScr = (TScr*)hScr;
    TI_UINT32   uResourceIndex;

    /* check both resources */
    for (uResourceIndex = 0; uResourceIndex < SCR_RESOURCE_NUM_OF_RESOURCES; uResourceIndex++)
    {
        /* if a client is currently running, notify it of the recovery event */
        if ( SCR_CID_NO_CLIENT != pScr->runningClient[ uResourceIndex ] )
        {
            TRACE1( pScr->hReport, REPORT_SEVERITY_INFORMATION, "FW reset occured. Client %d Notified.\n", pScr->runningClient[ uResourceIndex ]);
            if ( NULL != pScr->clientArray[ pScr->runningClient[ uResourceIndex ] ].clientRequestCB )
            {
                pScr->clientArray[ pScr->runningClient[ uResourceIndex ] ].clientRequestCB( pScr->clientArray[ pScr->runningClient[ uResourceIndex ] ].ClientRequestCBObj,
                                                                                          SCR_CRS_FW_RESET, (EScrResourceId)uResourceIndex, SCR_PR_NONE );
            }
            else
            {
                TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", pScr->runningClient[ uResourceIndex ]);
            }
        }
    #ifdef TI_DBG
        else
        {
            TRACE0( pScr->hReport, REPORT_SEVERITY_INFORMATION, "FW reset occured. No client was running.\n");
        }
    #endif
    }
}

/**
 * \\n
 * \date 27-April-2005\n
 * \brief Changes the current SCR group.\n
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param newGroup - the new group to use.\n
 */
void scr_setGroup( TI_HANDLE hScr, EScrGroupId newGroup )
{
    TScr            *pScr = (TScr*)hScr;
    TI_UINT32       i, uResourceIndex;
    EScrClientId    highestPending;

    TRACE1( pScr->hReport, REPORT_SEVERITY_INFORMATION, "Setting group %d.\n", newGroup);

#ifdef TI_DBG
    if (newGroup >= SCR_GID_NUM_OF_GROUPS)
    {
        TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Attempting to set invalid group to %d\n", newGroup);
        return;
    }
#endif

    /* keep the new group */
    pScr->currentGroup = newGroup;

    /* check both resources */
    for (uResourceIndex = 0; uResourceIndex < SCR_RESOURCE_NUM_OF_RESOURCES; uResourceIndex++)
    {
        /* for all pending clients */
        for ( i = 0; i < SCR_CID_NUM_OF_CLIENTS; i++ )
        {
            /* if the pending reason has escalated */
            if ( (pScr->clientArray[ i ].state[ uResourceIndex ] == SCR_CS_PENDING) && /* the client is pending */
                 (pScr->clientArray[ i ].currentPendingReason[ uResourceIndex ] < SCR_PR_DIFFERENT_GROUP_RUNNING) && /* the client was enabled in the previous group */
                 (TI_FALSE == clientStatus[ uResourceIndex ][ pScr->currentMode ][ newGroup ][ i ])) /* the client is not enabled in the new group */
            {
                /* mark the new pending reason */
                pScr->clientArray[ i ].currentPendingReason[ uResourceIndex ] = SCR_PR_DIFFERENT_GROUP_RUNNING;

                /* notify the client of the change, using its callback */
                if ( NULL != pScr->clientArray[ i ].clientRequestCB )
                {
                    pScr->clientArray[ i ].clientRequestCB( pScr->clientArray[ i ].ClientRequestCBObj, 
                                                            SCR_CRS_PEND, (EScrResourceId)uResourceIndex, SCR_PR_DIFFERENT_GROUP_RUNNING );
                }
                else
                {
                    TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", i);
                }
            }
        }
    

        /*
         * if no client is running call the highest pending client 
         * (after group change, previously pending clients may be enabled)
         */
        if ( SCR_CID_NO_CLIENT == pScr->runningClient[ uResourceIndex ] )
        {
            highestPending = scrFindHighest( hScr, SCR_CS_PENDING, uResourceIndex, 
                                             (SCR_CID_NUM_OF_CLIENTS - 1), 0 );
            if (( SCR_CID_NO_CLIENT != highestPending ) && (highestPending < SCR_CID_NUM_OF_CLIENTS))
            {
                pScr->clientArray[ highestPending ].state[ uResourceIndex ] = SCR_CS_RUNNING;
                pScr->clientArray[ highestPending ].currentPendingReason[ uResourceIndex ] = SCR_PR_NONE;
                pScr->runningClient[ uResourceIndex ] = (EScrClientId)highestPending;
                if ( NULL != pScr->clientArray[ highestPending ].clientRequestCB )
                {
                    pScr->clientArray[ highestPending ].clientRequestCB( pScr->clientArray[ highestPending ].ClientRequestCBObj, 
                                                                         SCR_CRS_RUN, (EScrResourceId)uResourceIndex, SCR_PR_NONE );
                }
                else
                {
                    TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", highestPending);
                }
            }
        }
    }
}

/**
 * \\n
 * \date 23-Nov-2005\n
 * \brief Changes the current SCR mode.\n
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param newMode - the new mode to use.\n
 */
void scr_setMode( TI_HANDLE hScr, EScrModeId newMode )
{
	TScr            *pScr = (TScr*)hScr;
#ifdef SCR_ABORT_NOTIFY_ENABLED
    TI_UINT32       i, uResourceIndex;
    EScrClientId    highestPending;
#endif

    TRACE1( pScr->hReport, REPORT_SEVERITY_INFORMATION, "Setting mode %d.\n", newMode);

#ifdef TI_DBG
    if (newMode >= SCR_MID_NUM_OF_MODES)
    {
        TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Attempting to set invalid mode to %d\n", newMode);
        return;
    }
#endif

    /* keep the new mode */
    pScr->currentMode = newMode;

#ifdef SCR_ABORT_NOTIFY_ENABLED

    for (uResourceIndex = 0; uResourceIndex < SCR_RESOURCE_NUM_OF_RESOURCES; uResourceIndex++)
    {
        /* Stage I : if someone is running and shouldn't be running in the new mode - Abort it */
        if ( (SCR_CID_NO_CLIENT != pScr->runningClient[ uResourceIndex ]) && 
             (TI_FALSE == clientStatus[ uResourceIndex ][ pScr->currentMode ][ pScr->currentGroup ][ pScr->runningClient[ uResourceIndex ] ]))
        {
            /* abort the running client */
            pScr->clientArray[ pScr->runningClient[ uResourceIndex ] ].state[ uResourceIndex ] = SCR_CS_ABORTING;
            if ( NULL != pScr->clientArray[ pScr->runningClient[ uResourceIndex ] ].clientRequestCB )
            {
                TRACE2( pScr->hReport, REPORT_SEVERITY_INFORMATION, "Sending abort request to client %d for resource %d\n", pScr->runningClient[ uResourceIndex ], uResourceIndex);
                pScr->clientArray[ pScr->runningClient[ uResourceIndex ] ].clientRequestCB( pScr->clientArray[ pScr->runningClient[ uResourceIndex ] ].ClientRequestCBObj,
                                                                                            SCR_CRS_ABORT,
                                                                                            (EScrResourceId)uResourceIndex,
                                                                                            SCR_PR_NONE );
            }
            else
            {
                TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", pScr->runningClient[ uResourceIndex ]);
            }
        }
    
        /* Stage II : notify escalated pending reason */
        /* for all pending clients */
        for ( i = 0; i < SCR_CID_NUM_OF_CLIENTS; i++ )
        {
            /* if the pending reason has escalated */
            if ( (pScr->clientArray[ i ].state[ uResourceIndex ] == SCR_CS_PENDING) && /* the client is pending */
                 (pScr->clientArray[ i ].currentPendingReason[ uResourceIndex ] < SCR_PR_DIFFERENT_GROUP_RUNNING) && /* the client was enabled in the previous group */
                 (TI_FALSE == clientStatus[ uResourceIndex ][ pScr->currentMode ][ pScr->currentGroup ][ i ])) /* the client is not enabled in the new group/mode */
            {
                /* mark the new pending reason */
                pScr->clientArray[ i ].currentPendingReason[ uResourceIndex ] = SCR_PR_DIFFERENT_GROUP_RUNNING;
                
                /* notify the client of the change, using its callback */
                if ( NULL != pScr->clientArray[ i ].clientRequestCB )
                {
                    pScr->clientArray[ i ].clientRequestCB( pScr->clientArray[ i ].ClientRequestCBObj, 
                                                            SCR_CRS_PEND, (EScrResourceId)uResourceIndex, 
                                                            SCR_PR_DIFFERENT_GROUP_RUNNING );
                }
                else
                {
                    TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", i);
                }
            }
        }
    
       
        /* Stage III : call Highest Pending Client who is enabled in the new mode   */
        if ( SCR_CID_NO_CLIENT == pScr->runningClient[ uResourceIndex ] )
        {
            highestPending = scrFindHighest( hScr, SCR_CS_PENDING, uResourceIndex, 
                                             (SCR_CID_NUM_OF_CLIENTS - 1), 0 );
            if (SCR_CID_NO_CLIENT != highestPending)
            {
                pScr->clientArray[ highestPending ].state[ uResourceIndex ] = SCR_CS_RUNNING;
                pScr->clientArray[ highestPending ].currentPendingReason[ uResourceIndex ] = SCR_PR_NONE;
                pScr->runningClient[ uResourceIndex ] = (EScrClientId)highestPending;
                if ( NULL != pScr->clientArray[ highestPending ].clientRequestCB )
                {
                    pScr->clientArray[ highestPending ].clientRequestCB( pScr->clientArray[ highestPending ].ClientRequestCBObj, 
                                                                         SCR_CRS_RUN, (EScrResourceId)uResourceIndex,
                                                                         SCR_PR_NONE );
                }
                else
                {
                    TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", highestPending);
                }
            }
        }
    }
#endif	/* SCR_ABORT_NOTIFY_ENABLED */ 
}


/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Request the channel use by a client
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param client - the client ID requesting the channel.\n
 * \param resource - the requested resource.\n
 * \param pPendReason - the reason for a pend reply.\n
 * \return The request status.\n
 * \retval SCR_CRS_REJECT the channel cannot be allocated to this client.
 * \retval SCR_CRS_PEND the channel is currently busy, and this client had been placed on the waiting list.
 * \retval SCR_CRS_RUN the channel is allocated to this client.
 */
EScrClientRequestStatus scr_clientRequest( TI_HANDLE hScr, EScrClientId client,
                                           EScrResourceId eResource, EScePendReason* pPendReason )
{
    TScr    *pScr = (TScr*)hScr;

    TRACE2( pScr->hReport, REPORT_SEVERITY_INFORMATION, "scr_clientRequest: Client %d requesting the channel for resource %d.\n", client, eResource);

#ifdef TI_DBG
    if (client >= SCR_CID_NUM_OF_CLIENTS)
    {
        TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Attempting to request SCR for invalid client %d\n", client);
        return SCR_CRS_PEND;
    }
    if (SCR_RESOURCE_NUM_OF_RESOURCES <= eResource)
    {
        TRACE2( pScr->hReport, REPORT_SEVERITY_ERROR, "Attempting to request SCR by client %d for invalid resource %d\n", client, eResource);
        return SCR_CRS_PEND;
    }
#endif
    
    *pPendReason = SCR_PR_NONE;

    /* check if already inside a request - shouldn't happen!!! */
    if ( TI_TRUE == pScr->statusNotficationPending )
    {
        TRACE0( pScr->hReport, REPORT_SEVERITY_ERROR, "request call while already in request!\n");
        return SCR_CRS_PEND;
    }

    /* check if current running client is requesting */
    if ( client == pScr->runningClient[ eResource ] )
    {
        TRACE2( pScr->hReport, REPORT_SEVERITY_WARNING, "Client %d re-requesting SCR for resource %d\n", client, eResource);
        return SCR_CRS_RUN;
    }

    TRACE5( pScr->hReport, REPORT_SEVERITY_INFORMATION, "scr_clientRequest: is client enabled = %d. eResource=%d,currentMode=%d,currentGroup=%d,client=%d,\n", clientStatus[ eResource ][ pScr->currentMode ][ pScr->currentGroup ][ client ], eResource, pScr->currentMode, pScr->currentGroup, client);

    /* check if the client is enabled in the current group */
    if ( TI_TRUE != clientStatus[ eResource ][ pScr->currentMode ][ pScr->currentGroup ][ client ])
    {
        pScr->clientArray[ client ].state[ eResource ] = SCR_CS_PENDING;
        pScr->clientArray[ client ].currentPendingReason[ eResource ]
                                                = *pPendReason = SCR_PR_DIFFERENT_GROUP_RUNNING;
        return SCR_CRS_PEND;
    }
        
    /* check if a there's no running client at the moment */
    if ( SCR_CID_NO_CLIENT == pScr->runningClient[ eResource ] )
    {
        /* no running or aborted client - allow access */
        TRACE2( pScr->hReport, REPORT_SEVERITY_INFORMATION, "Resource %d allocated to client: %d\n", eResource, client);
        pScr->clientArray[ client ].state[ eResource ] = SCR_CS_RUNNING;
        pScr->runningClient[ eResource ] = client;
        return SCR_CRS_RUN;
    }
    
    /* check if any client is aborting at the moment */
    if ( SCR_CS_ABORTING == pScr->clientArray[ pScr->runningClient[ eResource ] ].state[ eResource ] )
    {
        /* a client is currently aborting, but there still might be a pending client with higher priority
           than the client currently requesting the SCR. If such client exists, the requesting client is
           notified that it is pending because of this pending client, rather than the one currently aborting.
        */
        EScrClientId highestPending;
        highestPending = scrFindHighest( hScr, SCR_CS_PENDING, eResource, 
                                         (SCR_CID_NUM_OF_CLIENTS - 1), client );
        if ( (SCR_CID_NO_CLIENT == highestPending) ||
             (highestPending < client))
        {
            /* if the requesting client has higher priority than the current highest priority pending client,
               the current highest priority pending client should be notified that its pending reason has 
               changed (it is no longer waiting for current running client to abort, but for the requesting
               client to finish, once the current has aborted */
            if ( (highestPending != SCR_CID_NO_CLIENT) &&
                 (SCR_PR_OTHER_CLIENT_ABORTING == pScr->clientArray[ highestPending ].currentPendingReason[ eResource ]))
            {

                if ( NULL != pScr->clientArray[ highestPending ].clientRequestCB )
                {
                    pScr->clientArray[ highestPending ].clientRequestCB( pScr->clientArray[ highestPending ].ClientRequestCBObj,
                                                                         SCR_CRS_PEND, eResource, SCR_PR_OTHER_CLIENT_RUNNING );
                }
                else
                {
                    TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", highestPending);
                }
            }
            pScr->clientArray[ client ].currentPendingReason[ eResource ] = *pPendReason = SCR_PR_OTHER_CLIENT_ABORTING;
        }
        else
        {
            pScr->clientArray[ client ].currentPendingReason[ eResource ] = *pPendReason = SCR_PR_OTHER_CLIENT_RUNNING;
        }
        pScr->clientArray[ client ].state[ eResource ] = SCR_CS_PENDING;
        return SCR_CRS_PEND;
    }
 
    /* check if a client with higher priority is running */
    if (pScr->runningClient[ eResource ] > client)
    {
        pScr->clientArray[ client ].state[ eResource ] = SCR_CS_PENDING;
        pScr->clientArray[ client ].currentPendingReason[ eResource ] = *pPendReason = SCR_PR_OTHER_CLIENT_RUNNING;
        return SCR_CRS_PEND;
    }

    /* if the client is not supposed to abort lower priority clients */
    if ( (SCR_CID_NO_CLIENT == abortOthers[ eResource ][ client ]) || /* client is not supposed to abort any other client */
         (pScr->runningClient[ eResource ] > abortOthers[ eResource ][ client ])) /* client is not supposed to abort running client */
    {
        /* wait for the lower priority client */
        pScr->clientArray[ client ].state[ eResource ] = SCR_CS_PENDING;
        pScr->clientArray[ client ].currentPendingReason[ eResource ] = *pPendReason = SCR_PR_OTHER_CLIENT_RUNNING;
        return SCR_CRS_PEND;
    }

    /* at this point, there is a lower priority client running, that should be aborted: */
    /* mark the requesting client as pending (until the abort process will be completed) */
    pScr->clientArray[ client ].state[ eResource ] = SCR_CS_PENDING;

    /* mark that we are in the middle of a request (if a re-entrance will occur in the complete) */
    pScr->statusNotficationPending = TI_TRUE;

    /* abort the running client */
    pScr->clientArray[ pScr->runningClient[ eResource ] ].state[ eResource ] = SCR_CS_ABORTING;
    if ( NULL != pScr->clientArray[ pScr->runningClient[ eResource ] ].clientRequestCB )
    {
        TRACE2( pScr->hReport, REPORT_SEVERITY_INFORMATION, "Sending abort request to client %d for resource %d\n", pScr->runningClient[ eResource ], eResource);
        pScr->clientArray[ pScr->runningClient[ eResource ] ].clientRequestCB( pScr->clientArray[ pScr->runningClient[ eResource ] ].ClientRequestCBObj,
                                                                               SCR_CRS_ABORT, eResource,
                                                                               SCR_PR_NONE );
    }
    else
    {
        TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", pScr->runningClient[ eResource ]);
    }

    /* mark that we have finished the request process */
    pScr->statusNotficationPending = TI_FALSE;

    /* return the current status (in case the completion changed the client status to run) */
    if ( SCR_CS_RUNNING == pScr->clientArray[ client ].state[ eResource ] )
    {
        TRACE1( pScr->hReport, REPORT_SEVERITY_INFORMATION, "channel allocated to client: %d\n", client);
        return SCR_CRS_RUN;
    }
    else
    {
        pScr->clientArray[ client ].currentPendingReason[ eResource ] = *pPendReason = SCR_PR_OTHER_CLIENT_ABORTING;
        return SCR_CRS_PEND;
    }
}

/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Notifies the SCR that the client doe not require the channel any longer
 *
 * This function can be called both by clients that are in possession of the channel, and by
 * clients that are pending to use the channel.\n
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param client - the client releasing the channel.\n
 * \param eResource - the resource being released.\n
 * \return TI_OK if successful, TI_NOK otherwise.\n
 */
void scr_clientComplete( TI_HANDLE hScr, EScrClientId client, EScrResourceId eResource )
{
    TScr            *pScr = (TScr*)hScr;
    EScrClientId    highestPending;

    TRACE2( pScr->hReport, REPORT_SEVERITY_INFORMATION, "Client %d releasing resource %d.\n", client, eResource);

#ifdef TI_DBG
    if (client >= SCR_CID_NUM_OF_CLIENTS)
    {
        TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Attempting to release SCR for invalid client %d\n", client);
        return;
    }
    if (SCR_RESOURCE_NUM_OF_RESOURCES <= eResource)
    {
        TRACE2( pScr->hReport, REPORT_SEVERITY_ERROR, "Attempting to release invalid resource %d by client %d\n", eResource, client);
        return;
    }
#endif

    /* mark client state as idle */
    pScr->clientArray[ client ].state[ eResource ] = SCR_CS_IDLE;
    pScr->clientArray[ client ].currentPendingReason[ eResource ] = SCR_PR_NONE;

    /* if completing client is running (or aborting) */
    if ( pScr->runningClient[ eResource ] == client )   
    {
        /* mark no running client */
        pScr->runningClient[ eResource ] = SCR_CID_NO_CLIENT;

        /* find the pending client with highest priority */
        highestPending = scrFindHighest( hScr, SCR_CS_PENDING, eResource, (SCR_CID_NUM_OF_CLIENTS-1), 0 );
    
        /* if a pending client exists */
        if (( SCR_CID_NO_CLIENT != highestPending ) && (highestPending < SCR_CID_NUM_OF_CLIENTS))
        {
            /* mark the client with highest priority as running */
            pScr->clientArray[ highestPending ].state[ eResource ] = SCR_CS_RUNNING;
            pScr->clientArray[ highestPending ].currentPendingReason[ eResource ] = SCR_PR_NONE;
            pScr->runningClient[ eResource ] = highestPending;
        
            /* if the SCR is not called from within a client request (re-entrance) */
            if ( TI_FALSE == pScr->statusNotficationPending )
            {
                if ( NULL != pScr->clientArray[ highestPending ].clientRequestCB )
                {
                    pScr->clientArray[ highestPending ].clientRequestCB( pScr->clientArray[ highestPending ].ClientRequestCBObj,
                                                                         SCR_CRS_RUN, eResource, SCR_PR_NONE );
                }
                else
                {
                    TRACE1( pScr->hReport, REPORT_SEVERITY_ERROR, "Trying to call client %d callback, which is NULL\n", highestPending);
                }
            }
        }
    }
}

/**
 * \\n
 * \date 01-Dec-2004\n
 * \brief Searches the client database for a client with matching state, from startFrom to endAt (inclusive).
 * \brief Only searches for clients that are enabled at the current group!!!!\n
 *
 * Function Scope \e Private.\n
 * \param hScr - handle to the SCR object.\n
 * \param requiredState - the state to match.\n
 * \param eResource - the resource to macth.\n
 * \param startFrom - the highest priority to begin searching from.\n
 * \param endAt - the lowest priority to include in the search 
 * \return the client ID if found, SCR_CID_NO_CLIENT if not found.\n
 */
EScrClientId scrFindHighest( TI_HANDLE hScr,
                             EScrClientState requiredState,
                             EScrResourceId  eResource,
                             TI_UINT32 startFrom,
                             TI_UINT32 endAt )
{
    TScr        *pScr = (TScr*)hScr;
    TI_INT32    i, iStartFrom, iEndAt;

    /* 
     * signed indexes are used to avoid an overflow in the for loop when endAt equals zero
     * and the unsigned i is "reduced" to overflow to 4 Billion
     */
    iStartFrom = (TI_INT32)startFrom;
    iEndAt = (TI_INT32)endAt;

    /* loop on all clients, from start to end */
    for ( i = iStartFrom; i >= iEndAt; i-- )
    {
        /* check if the client state matches the required state */
        if ( (TI_TRUE == clientStatus[ eResource ][ pScr->currentMode ][ pScr->currentGroup ][ i ]) && /* client is enabled in current group */
             (requiredState == pScr->clientArray[ i ].state[ eResource ])) /* client is in required state */
        {
            /* and if so, return the client index */
            return (EScrClientId)i;
        }
    }

    return SCR_CID_NO_CLIENT;
}
