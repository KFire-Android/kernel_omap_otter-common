/*
 * scrDbg.c
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

/** \file  scrDbg.c
 *  \brief This file include the SCR debug module implementation
 *
 *  \see   scrDbg.h, scrApi.h
 */


#include "tidef.h"
#include "scrDbg.h"
#include "osApi.h"
#include "report.h"

#define MAX_DESC_LENGTH     50

char clientDesc[ SCR_CID_NO_CLIENT + 1 ][ MAX_DESC_LENGTH ] = 
{
    "SCR_CID_APP_SCAN",
    "SCR_CID_DRIVER_FG_SCAN",
    "SCR_CID_CONT_SCAN",
    "SCR_CID_XCC_MEASURE",
    "SCR_CID_BASIC_MEASURE",
    "SCR_CID_CONNECT",
    "SCR_CID_IMMED_SCAN",
    "SCR_CID_SWITCH_CHAN",
    "SCR_CID_NUM_OF_CLIENTS",
    "SCR_CID_NO_CLIENT"
};

char requestStatusDesc[ 4 ][ MAX_DESC_LENGTH ] = 
{
    "SCR_CRS_RUN",
    "SCR_CRS_PEND",
    "SCR_CRS_ABORT",
    "SCR_CRS_FW_RESET"
};

char pendReasonDesc[ SCR_PR_NONE + 1 ][ MAX_DESC_LENGTH ] = 
{
    "SCR_PR_OTHER_CLIENT_ABORTING",
    "SCR_PR_OTHER_CLIENT_RUNNING",
    "SCR_PR_DIFFERENT_GROUP_RUNNING",
    "SCR_PR_NONE"
};

char groupDesc[ SCR_GID_NUM_OF_GROUPS ][ MAX_DESC_LENGTH ] =
{
    "SCR_GID_IDLE",
    "SCR_GID_DRV_SCAN",
    "SCR_GID_APP_SCAN",
    "SCR_GID_CONNECT",
    "SCR_GID_CONNECTED",
    "SCR_GID_ROAMING"
};

char stateDesc[ SCR_CS_ABORTING + 1 ][ MAX_DESC_LENGTH ] = 
{
    "SCR_CS_IDLE",
    "SCR_CS_PENDING",
    "SCR_CS_RUNNING",
    "SCR_CS_ABORTING"
};

char modeDesc[ SCR_MID_NUM_OF_MODES][ MAX_DESC_LENGTH ] = 
{
    "SCR_MID_NORMAL",
    "SCR_MID_SG",
};

char resourceDesc[ SCR_RESOURCE_NUM_OF_RESOURCES ][ MAX_DESC_LENGTH ]=
{
    "SCR_RESOURCE_SERVING_CHANNEL",
    "SCR_RESOURCE_PERIODIC_SCAN"
};

/**
 * \\n
 * \date 01-May-2005\n
 * \brief Main SCR debug function
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param funcType - the specific debug function.\n
 * \param pParam - parameters for the debug function.\n
 */
void scrDebugFunction( TI_HANDLE hScr, TI_UINT32 funcType, void *pParam )
{
    switch (funcType)
    {
    case DBG_SCR_PRINT_HELP:
        printScrDbgFunctions();
        break;

    case DBG_SCR_CLIENT_REQUEST_SERVING_CHANNEL:
        requestAsClient( hScr, *((EScrClientId*)pParam), SCR_RESOURCE_SERVING_CHANNEL );  
        break;

    case DBG_SCR_CLIENT_RELEASE_SERVING_CHANNEL:
        releaseAsClient( hScr, *((EScrClientId*)pParam), SCR_RESOURCE_SERVING_CHANNEL );  
        break;

    case DBG_SCR_CLIENT_REQUEST_PERIODIC_SCAN:
        requestAsClient( hScr, *((EScrClientId*)pParam), SCR_RESOURCE_PERIODIC_SCAN ); 
        break;

    case DBG_SCR_CLIENT_RELEASE_PERIODIC_SCAN:
        releaseAsClient( hScr, *((EScrClientId*)pParam), SCR_RESOURCE_PERIODIC_SCAN ); 
        break;

    case DBG_SCR_SET_GROUP:
        changeGroup( hScr, *((EScrGroupId*)pParam) );
        break;

    case DBG_SCR_PRINT_OBJECT:
        printSCRObject( hScr );
        break;

    case DBG_SCR_SET_MODE:
        changeMode(hScr, *((EScrModeId*)pParam));
        break;

    default:
        WLAN_OS_REPORT(("Invalid function type in SCR debug function: %d\n", funcType));
        break;
    }
}

/**
 * \\n
 * \date 01-May-2005\n
 * \brief Prints SCR debug menu
 *
 * Function Scope \e Public.\n
 */
void printScrDbgFunctions(void)
{
    WLAN_OS_REPORT(("   SCR Debug Functions   \n"));
    WLAN_OS_REPORT(("-------------------------\n"));
    WLAN_OS_REPORT(("1700 - Print the SCR Debug Help\n"));
    WLAN_OS_REPORT(("1701 <client> - Request SCR as one shot scan (set client 0-7).\n"));
    WLAN_OS_REPORT(("1702 <client> - Release SCR as one shot scan (set client 0-7).\n"));
    WLAN_OS_REPORT(("1703 <client> - Request SCR as periodic scan (set client 0-7).\n"));
    WLAN_OS_REPORT(("1704 <client> - Release SCR as periodic scan (set client 0-7).\n"));
    WLAN_OS_REPORT(("1705 - Change SCR group\n"));
    WLAN_OS_REPORT(("1706 - Print SCR object\n"));
    WLAN_OS_REPORT(("1707 - Change SCR mode\n"));
}

/**
 * \\n
 * \date 29-March-2005\n
 * \brief Request the SCR with a given client ID.\n
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param client - the client to request as.\n\
 * \param eResource - the requested resource.\n
 */
void requestAsClient( TI_HANDLE hScr, EScrClientId client, EScrResourceId eResource )
{
    EScePendReason pendReason;
    EScrClientRequestStatus requestStatus;

    requestStatus = scr_clientRequest( hScr, client, eResource, &pendReason );
    WLAN_OS_REPORT(("Resource %s was requested as client %s, result %s, pend reason %s\n",
                    resourceDesc[ eResource ], clientDesc[ client ], requestStatusDesc[ requestStatus ],
                    pendReasonDesc[ pendReason ]));
}

/**
 * \\n
 * \date 01-May-2005\n
 * \brief Stops continuous scan process.\n
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param client - the client to release as.\n
 * \param eResource - the released resource.\n
 */
void releaseAsClient( TI_HANDLE hScr, EScrClientId client, EScrResourceId eResource )
{
    scr_clientComplete( hScr, client, eResource );
    WLAN_OS_REPORT(("Resource %s was released as client %s\n",
                    resourceDesc[ eResource ], clientDesc[ client ]));
}

/**
 * \\n
 * \date 01-May-2005\n
 * \brief Change the SCR group.\n
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param group - the group to change to.\n
 */
void changeGroup( TI_HANDLE hScr, EScrGroupId group )
{
    scr_setGroup( hScr, group );
    WLAN_OS_REPORT(("SCR group was changed to %s\n",
                    groupDesc[ group ]));
}

/**
 * \\n
 * \date 23-Nov-2005\n
 * \brief Change the SCR mode.\n
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 * \param mode - the mode to change to.\n
 */
void changeMode( TI_HANDLE hScr, EScrModeId mode )
{
    scr_setMode( hScr, mode );
    WLAN_OS_REPORT(("SCR mode was changed to %s\n",
                    modeDesc[ mode ]));
}
/**
 * \\n
 * \date 15-June-2005\n
 * \brief Prints the SCR object.\n
 *
 * Function Scope \e Public.\n
 * \param hScr - handle to the SCR object.\n
 */
void printSCRObject( TI_HANDLE hScr )
{
#ifdef REPORT_LOG
    TScr* pScr = (TScr*)hScr;
    int i;

    WLAN_OS_REPORT( ("SCR current group:%s, mode: %s, serving channel owner:%s, periodic scan owner: %s "
                     "within request:%s\n", 
                     groupDesc[ pScr->currentGroup ],modeDesc[ pScr->currentMode ],
                     clientDesc[ pScr->runningClient[ SCR_RESOURCE_SERVING_CHANNEL ] ],
                     clientDesc[ pScr->runningClient[ SCR_RESOURCE_PERIODIC_SCAN ] ],
                     (TI_TRUE == pScr->statusNotficationPending ? "Yes" : "No" )) );
    
    WLAN_OS_REPORT( ("%-22s %-15s %-15s %-15s %-15s\n", "Client", "State (SC)", "State (PS)", "Pend Reason (SC)", "Pend Reason (PS)") );
    WLAN_OS_REPORT( ("----------------------------------------------------------------------\n"));
    for ( i = 0; i < SCR_CID_NUM_OF_CLIENTS; i++ )
    {
        WLAN_OS_REPORT( ("%-22s %-15s %-15s %-15s %-15s \n",
                         clientDesc[ i ], stateDesc[ pScr->clientArray[ i ].state[ SCR_RESOURCE_SERVING_CHANNEL ] ], 
                         stateDesc[ pScr->clientArray[ i ].state[ SCR_RESOURCE_PERIODIC_SCAN ] ],
                         pendReasonDesc[ pScr->clientArray[ i ].currentPendingReason[ SCR_RESOURCE_SERVING_CHANNEL ] ],
                         pendReasonDesc[ pScr->clientArray[ i ].currentPendingReason[ SCR_RESOURCE_PERIODIC_SCAN ] ]) );
    }
#endif
}
