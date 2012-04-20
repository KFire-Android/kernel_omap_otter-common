/*
 * smeSm.c
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

/** \file  smeSm.c
 *  \brief SME state machine implementation
 *
 *  \see   smeSm.h, sme.c, sme.h
 */


#define __FILE_ID__  FILE_ID_43
#include "GenSM.h"
#include "smeSm.h"
#include "smePrivate.h"
#include "connApi.h"
#include "apConn.h"
#include "ScanCncn.h"
#include "scanResultTable.h"
#include "EvHandler.h"
#include "regulatoryDomainApi.h"
#include "siteMgrApi.h"
#include "DrvMain.h"


static OS_802_11_DISASSOCIATE_REASON_E eDisassocConvertTable[ MGMT_STATUS_MAX_NUM +1] = 
{
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_AUTH_REJECT,
    OS_DISASSOC_STATUS_ASSOC_REJECT,
    OS_DISASSOC_STATUS_SECURITY_FAILURE,
    OS_DISASSOC_STATUS_AP_DEAUTHENTICATE,
    OS_DISASSOC_STATUS_AP_DISASSOCIATE,
    OS_DISASSOC_STATUS_ROAMING_TRIGGER,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED,
    OS_DISASSOC_STATUS_UNSPECIFIED
};

#define SME_CONVERT_DISASSOC_CODES(disassocReason)     (eDisassocConvertTable[ (disassocReason) ]) 

static void smeSm_Start (TI_HANDLE hSme);
static void smeSm_Stop (TI_HANDLE hSme);
static void smeSm_PreConnect (TI_HANDLE hSme);
static void smeSm_Connect (TI_HANDLE hSme);
static void smeSm_ConnectSuccess (TI_HANDLE hSme);
static void smeSm_Disconnect (TI_HANDLE hSme);
static void smeSm_DisconnectDone (TI_HANDLE hSme);
static void smeSm_StopScan (TI_HANDLE hSme);
static void smeSm_StopConnect (TI_HANDLE hSme);
static void smeSm_ConnWhenConnecting (TI_HANDLE hSme);
static void smeSm_ActionUnexpected (TI_HANDLE hSme);
static void smeSm_NopAction (TI_HANDLE hSme);
static void smeSm_CheckStartConditions (TI_HANDLE hSme);

static TI_STATUS sme_StartScan (TI_HANDLE hSme);
static void sme_updateScanCycles (TI_HANDLE hSme,
                                  TI_BOOL bDEnabled,
                                  TI_BOOL bCountryValid,
                                  TI_BOOL bConstantScan);
static void sme_CalculateCyclesNumber (TI_HANDLE hSme, TI_UINT32 uTotalTimeMs);

TGenSM_actionCell tSmMatrix[ SME_SM_NUMBER_OF_STATES ][ SME_SM_NUMBER_OF_EVENTS ] = 
    {
        { /* SME_SM_STATE_IDLE */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_Start },             /* SME_SM_EVENT_START */
            { SME_SM_STATE_IDLE, smeSm_ActionUnexpected },          /* SME_SM_EVENT_STOP */
            { SME_SM_STATE_IDLE, smeSm_ActionUnexpected },          /* SME_SM_EVENT_CONNECT */
            { SME_SM_STATE_IDLE, smeSm_ActionUnexpected },          /* SME_SM_EVENT_CONNECT_SUCCESS */
            { SME_SM_STATE_IDLE, smeSm_ActionUnexpected },          /* SME_SM_EVENT_CONNECT_FAILURE */ 
            { SME_SM_STATE_IDLE, smeSm_CheckStartConditions },          /* SME_SM_EVENT_DISCONNECT */
        },
        { /* SME_SM_STATE_WAIT_CONNECT */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_ActionUnexpected },  /* SME_SM_EVENT_START */
            { SME_SM_STATE_IDLE, smeSm_Stop },                      /* SME_SM_EVENT_STOP */
            { SME_SM_STATE_SCANNING, smeSm_PreConnect },            /* SME_SM_EVENT_CONNECT */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_ActionUnexpected },  /* SME_SM_EVENT_CONNECT_SUCCESS */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_ActionUnexpected },  /* SME_SM_EVENT_CONNECT_FAILURE */ 
            { SME_SM_STATE_WAIT_CONNECT, smeSm_Start },             /* SME_SM_EVENT_DISCONNECT */
        },
        { /* SME_SM_STATE_SCANNING */
            { SME_SM_STATE_SCANNING, smeSm_ActionUnexpected },      /* SME_SM_EVENT_START */
            { SME_SM_STATE_DISCONNECTING, smeSm_StopScan },         /* SME_SM_EVENT_STOP */
            { SME_SM_STATE_CONNECTING, smeSm_Connect },             /* SME_SM_EVENT_CONNECT */
            { SME_SM_STATE_SCANNING, smeSm_ActionUnexpected },      /* SME_SM_EVENT_CONNECT_SUCCESS */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_DisconnectDone },    /* SME_SM_EVENT_CONNECT_FAILURE */ 
            { SME_SM_STATE_DISCONNECTING, smeSm_StopScan },         /* SME_SM_EVENT_DISCONNECT */
        },
        { /* SME_SM_STATE_CONNECTING */
            { SME_SM_STATE_CONNECTING, smeSm_ActionUnexpected },    /* SME_SM_EVENT_START */
            { SME_SM_STATE_DISCONNECTING, smeSm_StopConnect },      /* SME_SM_EVENT_STOP */
            { SME_SM_STATE_CONNECTING, smeSm_ConnWhenConnecting },  /* SME_SM_EVENT_CONNECT */
            { SME_SM_STATE_CONNECTED, smeSm_ConnectSuccess },       /* SME_SM_EVENT_CONNECT_SUCCESS */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_DisconnectDone },    /* SME_SM_EVENT_CONNECT_FAILURE */ 
            { SME_SM_STATE_DISCONNECTING, smeSm_StopConnect },      /* SME_SM_EVENT_DISCONNECT */
        },
        { /* SME_SM_STATE_CONNECTED */
            { SME_SM_STATE_CONNECTED, smeSm_ActionUnexpected },     /* SME_SM_EVENT_START */
            { SME_SM_STATE_DISCONNECTING, smeSm_Disconnect },       /* SME_SM_EVENT_STOP */
            { SME_SM_STATE_CONNECTED, smeSm_ActionUnexpected },     /* SME_SM_EVENT_CONNECT */
            { SME_SM_STATE_CONNECTED, smeSm_ActionUnexpected },     /* SME_SM_EVENT_CONNECT_SUCCESS */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_DisconnectDone },    /* SME_SM_EVENT_CONNECT_FAILURE */ 
            { SME_SM_STATE_DISCONNECTING, smeSm_Disconnect },       /* SME_SM_EVENT_DISCONNECT */
        },
        { /* SME_SM_STATE_DISCONNECTING */
            { SME_SM_STATE_DISCONNECTING, smeSm_ActionUnexpected }, /* SME_SM_EVENT_START */
            { SME_SM_STATE_DISCONNECTING, smeSm_ActionUnexpected }, /* SME_SM_EVENT_STOP */
            { SME_SM_STATE_DISCONNECTING, smeSm_ActionUnexpected }, /* SME_SM_EVENT_CONNECT */
            { SME_SM_STATE_DISCONNECTING, smeSm_ActionUnexpected }, /* SME_SM_EVENT_CONNECT_SUCCESS */
            { SME_SM_STATE_WAIT_CONNECT, smeSm_DisconnectDone },    /* SME_SM_EVENT_CONNECT_FAILURE */ 
            { SME_SM_STATE_DISCONNECTING, smeSm_NopAction }, /* SME_SM_EVENT_DISCONNECT */
        }
    };

TI_INT8*  uStateDescription[] = 
    {
        "IDLE",
        "WAIT_CONNECT",
        "SCANNING",
        "CONNECTING",
        "CONNECTED",
        "DISCONNECTING"
    };

TI_INT8*  uEventDescription[] = 
    {
        "START",
        "STOP",
        "CONNECT",
        "CONNECT_SUCCESS",
        "CONNECT_FAILURE",
        "DISCONNECT"
    };

/** 
 * \fn     smeSm_Start
 * \brief  Starts STA opeartion by moving SCR out of idle group and starting connection process
 * 
 * Starts STA opeartion by moving SCR out of idle group and starting connection process
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_Stop, sme_start 
 */ 
void smeSm_Start (TI_HANDLE hSme)
{
    TSme    *pSme = (TSme*)hSme;

    /* set SCR group according to connection mode */
    if (CONNECT_MODE_AUTO == pSme->eConnectMode)
    {
        TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "smeSm_Start: changing SCR group to DRV scan\n");
        scr_setGroup (pSme->hScr, SCR_GID_DRV_SCAN);
    }
    else
    {
        TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "smeSm_Start: changing SCR group to APP scan\n");
        scr_setGroup (pSme->hScr, SCR_GID_APP_SCAN);
    }

    if ((TI_FALSE == pSme->bRadioOn) || (TI_FALSE == pSme->bRunning)) 
    {
        /* Radio is off so send stop event */
        sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_STOP, hSme);
    }
    else if (TI_TRUE == pSme->bConnectRequired)
    {
        /* if connection was required, start the process */
        sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT, hSme);
    }
}

/** 
 * \fn     smeSm_Stop
 * \brief  Turns off the STA
 * 
 * Turns off the STA by moving the SCr to idle
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_Start, sme_Stop
 */ 
void smeSm_Stop (TI_HANDLE hSme)
{
    TSme    *pSme = (TSme*)hSme;

    /* set SCR group to idle */
    scr_setGroup (pSme->hScr, SCR_GID_IDLE);

    if (TI_FALSE == pSme->bRunning)
    {
        /* call DrvMain */
        drvMain_SmeStop (pSme->hDrvMain);
    }
}

/** 
 * \fn     smeSm_PreConnect
 * \brief  Initiates the connection process
 * 
 * Initiates the connection process - for automatic mode, start scan, for manual mode - triggers connection
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_Connect, smeSm_ConnectSuccess
 */ 
void smeSm_PreConnect (TI_HANDLE hSme)
{
    TSme *pSme = (TSme *)hSme;
    paramInfo_t	*pParam;

    /* set the connection mode with which this connection attempt is starting */
    pSme->eLastConnectMode = pSme->eConnectMode;
 
    /* mark that no authentication/assocaition was yet sent */
    pSme->bAuthSent = TI_FALSE;

    /* try to find a connection candidate (manual mode have already performed scann */
    pSme->pCandidate = sme_Select (hSme);
    if (NULL != pSme->pCandidate)
    {
        /* candidate is available - attempt connection */
        sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT, hSme);
    }
    /* no candidate */
    else
    {
        if (CONNECT_MODE_AUTO == pSme->eConnectMode)
        {
            /* automatic mode - start scanning */
            if (TI_OK != sme_StartScan (hSme))
            {
                TRACE0(pSme->hReport, REPORT_SEVERITY_ERROR , "smeSm_PreConnect: unable to start scan, stopping the SME\n");
                pSme->bRadioOn = TI_FALSE;
                sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT_FAILURE, hSme);
            }

            /* update scan count counter */
            if(pSme->uScanCount < PERIODIC_SCAN_MAX_INTERVAL_NUM)
            {
                pSme->uScanCount++;
            }

        }
        else		/* Manual mode */
        {
			/* for IBSS or any, if no entries where found, add the self site */
			if (pSme->eBssType == BSS_INFRASTRUCTURE)
            {
                /* makr whether we need to stop the attempt connection in manual mode */
                pSme->bConnectRequired = TI_FALSE;

				TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "smeSm_PreConnect: No candidate available, sending connect failure\n");
                /* manual mode and no connection candidate is available - connection failed */
                sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT_FAILURE, hSme);
			}

			else		/* IBSS */
			{
				TI_UINT8    uDesiredChannel;
                TI_BOOL     channelValidity;

		        pSme->bConnectRequired = TI_FALSE;

                pParam = (paramInfo_t *)os_memoryAlloc(pSme->hOS, sizeof(paramInfo_t));
                if (!pParam)
                {
                    return;
                }

				pParam->paramType = SITE_MGR_DESIRED_CHANNEL_PARAM;
				siteMgr_getParam(pSme->hSiteMgr, pParam);
				uDesiredChannel = pParam->content.siteMgrDesiredChannel;

				if (uDesiredChannel >= SITE_MGR_CHANNEL_A_MIN)
				{
				   pParam->content.channelCapabilityReq.band = RADIO_BAND_5_0_GHZ;
				}
				else
				{
				   pParam->content.channelCapabilityReq.band = RADIO_BAND_2_4_GHZ;
				}

				/*
				update the regulatory domain with the selected band
				*/
				/* Check if the selected channel is valid according to regDomain */
				pParam->paramType = REGULATORY_DOMAIN_GET_SCAN_CAPABILITIES;
				pParam->content.channelCapabilityReq.scanOption = ACTIVE_SCANNING;
				pParam->content.channelCapabilityReq.channelNum = uDesiredChannel;

				regulatoryDomain_getParam (pSme->hRegDomain, pParam);
                channelValidity = pParam->content.channelCapabilityRet.channelValidity;
                os_memoryFree(pSme->hOS, pParam, sizeof(paramInfo_t));
				if (!channelValidity)
				{
				   TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "IBSS SELECT FAILURE  - No channel !!!\n\n");

				   sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT_FAILURE, hSme);

				   return;
				}

				pSme->pCandidate = (TSiteEntry *)addSelfSite(pSme->hSiteMgr);

				if (pSme->pCandidate == NULL)
				{
				   TRACE0(pSme->hReport, REPORT_SEVERITY_ERROR , "IBSS SELECT FAILURE  - could not open self site !!!\n\n");

				   sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT_FAILURE, hSme);

				   return;
				}

#ifdef REPORT_LOG
				TRACE6(pSme->hReport, REPORT_SEVERITY_CONSOLE,"%%%%%%%%%%%%%%	SELF SELECT SUCCESS, bssid: %X-%X-%X-%X-%X-%X	%%%%%%%%%%%%%%\n\n", pSme->pCandidate->bssid[0], pSme->pCandidate->bssid[1], pSme->pCandidate->bssid[2], pSme->pCandidate->bssid[3], pSme->pCandidate->bssid[4], pSme->pCandidate->bssid[5]);
                WLAN_OS_REPORT (("%%%%%%%%%%%%%%	SELF SELECT SUCCESS, bssid: %02x.%02x.%02x.%02x.%02x.%02x %%%%%%%%%%%%%%\n\n", pSme->pCandidate->bssid[0], pSme->pCandidate->bssid[1], pSme->pCandidate->bssid[2], pSme->pCandidate->bssid[3], pSme->pCandidate->bssid[4], pSme->pCandidate->bssid[5]));
#endif
				/* a connection candidate is available, send a connect event */
				sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT, hSme);
			}
        }
    }
}

/** 
 * \fn     smeSm_Connect
 * \brief  Starts a connection process with the selected network
 * 
 * Starts a connection process with the selected network
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_PreConnect, smeSm_ConnectSuccess
 */ 
void smeSm_Connect (TI_HANDLE hSme)
{
    TSme            *pSme = (TSme*)hSme;
    TI_STATUS       tStatus;
    paramInfo_t     *pParam;

    /* Sanity check - if no connection candidate was found so far */
    if (NULL == pSme->pCandidate)
    {
        TRACE0(pSme->hReport, REPORT_SEVERITY_ERROR , "smeSm_Connect: No candidate available, sending connect failure\n");
        sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_CONNECT_FAILURE, hSme);
    }
    else
    {
        pParam = (paramInfo_t *)os_memoryAlloc(pSme->hOS, sizeof(paramInfo_t));
        if (!pParam)
        {
            return;
        }

       /* set SCR group */
       if (BSS_INFRASTRUCTURE == pSme->pCandidate->bssType)
       {
           scr_setGroup (pSme->hScr, SCR_GID_CONNECT);
       }

       /***************** Config Connection *************************/
       pParam->paramType = CONN_TYPE_PARAM;	
       if (BSS_INDEPENDENT == pSme->pCandidate->bssType)
           if (SITE_SELF == pSme->pCandidate->siteType)
           {
               pParam->content.connType = CONNECTION_SELF;
           }
           else
           {
               pParam->content.connType = CONNECTION_IBSS;
           }
       else
           pParam->content.connType = CONNECTION_INFRA;
       conn_setParam(pSme->hConn, pParam);
       os_memoryFree(pSme->hOS, pParam, sizeof(paramInfo_t));

       /* start the connection process */
       tStatus = conn_start (pSme->hConn, CONN_TYPE_FIRST_CONN, sme_ReportConnStatus, hSme, TI_FALSE, TI_FALSE);
       if (TI_OK != tStatus)
       {
           TRACE1(pSme->hReport, REPORT_SEVERITY_ERROR , "smeSm_Connect: conn_start returned status %d\n", tStatus);
       }
    }
}

/** 
 * \fn     smeSm_ConnectSuccess
 * \brief  Handles connection success indication
 * 
 * Handles connection success indication - starts AP conn and set SCR group to connected
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_PreConnect, smeSm_Connect
 */ 
void smeSm_ConnectSuccess (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;

    pSme->uScanCount = 0;

    /* connection succedded to the connection candidate - start AP connection */
    if (BSS_INFRASTRUCTURE == pSme->pCandidate->bssType)
    {
        /* Start the AP connection */
        apConn_start (pSme->hApConn, 
                      (pSme->tSsid.len != 0) && !OS_802_11_SSID_JUNK (pSme->tSsid.str, pSme->tSsid.len));
    }

    /* Set SCR group to connected */
    scr_setGroup (pSme->hScr, SCR_GID_CONNECTED);
}

/** 
 * \fn     smeSm_Disconnect
 * \brief  Starts a disconnect by calling the AP connection or connect modules
 * 
 * Starts a disconnect by calling the AP connection or connect modules
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_DisconnectDone
 */ 
void smeSm_Disconnect (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;
    TI_STATUS   tStatus;

    /* set the SCr group to connecting */
    scr_setGroup (pSme->hScr, SCR_GID_CONNECT);

    if (BSS_INFRASTRUCTURE == pSme->pCandidate->bssType)
    {
		 /* Call the AP connection to perform disconnect */
		 tStatus = apConn_stop (pSme->hApConn, TI_TRUE);
	}
	else 
	{
	    /* In IBSS disconnect is done directly with the connection SM */ 
		tStatus = conn_stop(pSme->hConn, DISCONNECT_DE_AUTH, STATUS_UNSPECIFIED,
						   TI_TRUE, sme_ReportConnStatus, hSme);
		if (tStatus != TI_OK)
		{
TRACE1(pSme->hReport, REPORT_SEVERITY_ERROR , "smeSm_Disconnect: conn_stop retruned %d\n", tStatus);
		}
    }
}

/** 
 * \fn     smeSm_DisconnectDone
 * \brief  Finish a disconnect process
 * 
 * Finish a disconnect process by sending the appropriate event and restarting the state-machine
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_Disconnect
 */ 
void smeSm_DisconnectDone (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;
    OS_802_11_DISASSOCIATE_REASON_T	    tEventReason;

    if (TI_FALSE == pSme->bReselect)
    {
        /* send an event notifying the disassocation */
        if (TI_TRUE == pSme->bAuthSent)
        {
            tEventReason.eDisAssocType = SME_CONVERT_DISASSOC_CODES (pSme->tDisAssoc.eMgmtStatus);
            tEventReason.uStatusCode = pSme->tDisAssoc.uStatusCode;
            EvHandlerSendEvent (pSme->hEvHandler, IPC_EVENT_DISASSOCIATED, (TI_UINT8*)&tEventReason,
                                sizeof(OS_802_11_DISASSOCIATE_REASON_T));
        }
        else if (CONNECT_MODE_AUTO != pSme->eLastConnectMode)
        {
            EvHandlerSendEvent (pSme->hEvHandler, IPC_EVENT_NOT_ASSOCIATED, NULL, 0);
        }
    }

    siteMgr_disSelectSite (pSme->hSiteMgr);
    
    /* try to reconnect */
    smeSm_Start (hSme);
}

/** 
 * \fn     smeSm_StopScan
 * \brief  Stops the SME scan operation
 * 
 * Stops the SME scan operation
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_PreConnect, sme_StartScan
 */ 
void smeSm_StopScan (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;

    scanCncn_StopPeriodicScan (pSme->hScanCncn, SCAN_SCC_DRIVER);
}

/** 
 * \fn     smeSm_StopConnect
 * \brief  Stops the connect module
 * 
 * Stops the connect module (if the SME is stopped during a connect attempt
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_Connect
 */ 
void smeSm_StopConnect (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;
    TI_STATUS   tStatus;

    tStatus = conn_stop (pSme->hConn, DISCONNECT_DE_AUTH, STATUS_UNSPECIFIED,
                         TI_TRUE, sme_ReportConnStatus, hSme);

    if (TI_OK != tStatus)
    {
        TRACE1(pSme->hReport, REPORT_SEVERITY_ERROR , "smeSm_StopConnect: conn_stop returned status %d\n", tStatus);
    }
}

/** 
 * \fn     smeSm_ConnWhenConnecting 
 * \brief  Starts the connect process again
 * 
 * Starts the connect process again
 * 
 * \param  hSme - handle to the SME object
 * \return None
 * \sa     smeSm_Connect
 */ 
void smeSm_ConnWhenConnecting (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;
    TI_STATUS   tStatus;

    /* start the connection process */
    tStatus = conn_start (pSme->hConn, CONN_TYPE_FIRST_CONN, sme_ReportConnStatus, hSme, TI_FALSE, TI_FALSE);
    if (TI_OK != tStatus)
    {
        TRACE1(pSme->hReport, REPORT_SEVERITY_ERROR , "smeSm_ConnWhenConnecting: conn_start returned status %d\n", tStatus);
    }
}

/** 
 * \fn     smeSm_ActionUnexpected
 * \brief  Called when an unexpected event (for current state) is received
 * 
 * Called when an unexpected event (for current state) is received
 * 
 * \param  hSme - handle to the SME object
 * \return None
 */ 
void smeSm_ActionUnexpected (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;

    TRACE0(pSme->hReport, REPORT_SEVERITY_ERROR , "smeSm_ActionUnexpected called\n");
}

/** 
 * \fn     smeSm_NopAction
 * \brief  Called when event call and don't need to do nothing.
 * 
 * \param  hSme - handle to the SME object
 * \return None
 */ 
void smeSm_NopAction (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;

    TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "smeSm_NopAction called\n");
}

void smeSm_CheckStartConditions (TI_HANDLE hSme)
{
    TSme        *pSme = (TSme*)hSme;

    if ((TI_TRUE == pSme->bRunning) && (TI_TRUE == pSme->bRadioOn))
    {
        /* send a start event */
        sme_SmEvent (pSme->hSmeSm, SME_SM_EVENT_START, hSme);
    }
}


/* do we need to verify G only / A only / dual-band with site mgr? or rely on channels only? */

/** 
 * \fn     sme_StartScan
 * \brief  Set scan parameters and calls scan concnetartor to start the scan operation.
 * 
 * Set scan parameters and calls scan concnetartor to start the scan operation.
 * 
 * Scan parameters are set according to scan target - find country IE, find desired SSID, or both
 * (one on each band). To find country IE we use passive scan forever, to find the desired SSID we
 * use active scan until the current country IE expires. In addition, we take into account the WSC PB
 * mode - scan constantly for two minutes (but under the country validity and expiry constraints)
 * 
 * \param  hSme - handle to the SME object
 * \return TI_OK if scan started successfully, TI_NOK otherwise
 * \sa     smeSm_PreConnect
 */ 
TI_STATUS sme_StartScan (TI_HANDLE hSme)
{
    TSme            *pSme = (TSme*)hSme;
    paramInfo_t     *pParam;
    TI_BOOL         bDEnabled, bCountryValid;
    TI_BOOL         bBandChannelExist[ RADIO_BAND_NUM_OF_BANDS ];
    TI_BOOL         bBandCountryFound[ RADIO_BAND_NUM_OF_BANDS ];
    TI_STATUS       tStatus;
    TI_UINT32       uIndex;

    /* get 802.11d enable state */
    pParam = (paramInfo_t *)os_memoryAlloc(pSme->hOS, sizeof(paramInfo_t));
    if (!pParam)
    {
        return TI_NOK;
    }

    pParam->paramType = REGULATORY_DOMAIN_ENABLED_PARAM;
    regulatoryDomain_getParam (pSme->hRegDomain, pParam);
    bDEnabled = pParam->content.regulatoryDomainEnabled;

    pParam->paramType = REGULATORY_DOMAIN_IS_COUNTRY_FOUND;
    /* get country validity for all bands */
    for (uIndex = 0; uIndex < RADIO_BAND_NUM_OF_BANDS; uIndex++)
    {
        pParam->content.eRadioBand = (ERadioBand)uIndex;
        regulatoryDomain_getParam (pSme->hRegDomain, pParam);
        bBandCountryFound[ uIndex ] = pParam->content.bIsCountryFound;
        /* also nullify the channel exist indication for this band */
        bBandChannelExist[ uIndex ] = TI_FALSE;
    }
    os_memoryFree(pSme->hOS, pParam, sizeof(paramInfo_t));

    /* First fill the channels */
    for (uIndex = 0; uIndex < pSme->tInitParams.uChannelNum; uIndex++)
    {
        /* for each channel, if country is found, set active scan */
        pSme->tScanParams.tChannels[ uIndex ].eBand = pSme->tInitParams.tChannelList[ uIndex ].eBand;
        pSme->tScanParams.tChannels[ uIndex ].uChannel = pSme->tInitParams.tChannelList[ uIndex ].uChannel;
        pSme->tScanParams.tChannels[ uIndex ].uMaxDwellTimeMs = pSme->tInitParams.uMaxScanDuration;
        pSme->tScanParams.tChannels[ uIndex ].uMinDwellTimeMs = pSme->tInitParams.uMinScanDuration;
        pSme->tScanParams.tChannels[ uIndex ].uTxPowerLevelDbm = DEF_TX_POWER;

        /* if 802.11d is disabled, or country is available for this band */
        if ((TI_FALSE == bDEnabled) || 
            (TI_TRUE == bBandCountryFound[ pSme->tInitParams.tChannelList[ uIndex ].eBand ]))
        {
            /* set active scan */
            pSme->tScanParams.tChannels[ uIndex ].eScanType = SCAN_TYPE_NORMAL_ACTIVE;
        }
        /* 802.11d is enabled and no country available */
        else
        {
            /* set passive scan */
            pSme->tScanParams.tChannels[ uIndex ].eScanType = SCAN_TYPE_NORMAL_PASSIVE;

            /* 
             * in order to fined country set uMaxDwellTimeMs ( that at passive scan set the passiveScanDuration ) 
             * to significant value 
             */
            pSme->tScanParams.tChannels[ uIndex ].uMaxDwellTimeMs = SCAN_CNCN_REGULATORY_DOMAIN_PASSIVE_DWELL_TIME_DEF;
        }
        /* mark that a channel exists for this band */
        bBandChannelExist[ pSme->tInitParams.tChannelList[ uIndex ].eBand ] = TI_TRUE;
    }
    /* set number of channels */
    pSme->tScanParams.uChannelNum = pSme->tInitParams.uChannelNum;

    /* now, fill global parameters */
    pSme->tScanParams.uProbeRequestNum = pSme->tInitParams.uProbeReqNum;
    pSme->tScanParams.iRssiThreshold = pSme->tInitParams.iRssiThreshold;
    pSme->tScanParams.iSnrThreshold = pSme->tInitParams.iSnrThreshold;
    pSme->tScanParams.bTerminateOnReport = TI_TRUE;
    pSme->tScanParams.uFrameCountReportThreshold = 1; 

    /* 
     * if for at least one band country is known and scan is performed on this band - means we need to 
     * take into consideration country expiry, plus we are scanning for the desired SSID
     */
    bCountryValid = ((TI_TRUE == bBandChannelExist[ RADIO_BAND_2_4_GHZ ]) && (TI_TRUE == bBandCountryFound[ RADIO_BAND_2_4_GHZ ])) ||
                    ((TI_TRUE == bBandChannelExist[ RADIO_BAND_5_0_GHZ ]) && (TI_TRUE == bBandCountryFound[ RADIO_BAND_5_0_GHZ ]));

    /* set SSID(s) and BSS type according to 802.11d status, and country availability */
    /* if 802.11d is disabled */
    if (TI_FALSE == bDEnabled)
    {
        pSme->tScanParams.eBssType = pSme->eBssType;
        /* set the deisred SSID, or any SSID if this is the desired SSID */
        if (SSID_TYPE_ANY == pSme->eSsidType)
        {
            pSme->tScanParams.uSsidNum = 0;
            pSme->tScanParams.uSsidListFilterEnabled = 1;
        }
        else
        {
            pSme->tScanParams.tDesiredSsid[ 0 ].eVisability = SCAN_SSID_VISABILITY_HIDDEN;
            os_memoryCopy (pSme->hOS, &(pSme->tScanParams.tDesiredSsid[ 0 ].tSsid), &(pSme->tSsid), sizeof (TSsid));
            pSme->tScanParams.uSsidNum = 1;
            pSme->tScanParams.uSsidListFilterEnabled = 1;

#ifdef XCC_MODULE_INCLUDED
            pSme->tScanParams.uSsidListFilterEnabled = (TI_UINT8)TI_FALSE;
            pSme->tScanParams.uSsidNum = 2;
            pSme->tScanParams.tDesiredSsid[ 1 ].tSsid.len = 0;
            pSme->tScanParams.tDesiredSsid[ 1 ].eVisability =  SCAN_SSID_VISABILITY_PUBLIC;
#endif

        }
    }
    /* Country code exists and scan is performed on this band - take country expiry timr into account */
    else if (TI_TRUE == bCountryValid)
    {
        TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_StartScan: performing active scan to find desired SSID\n");

        /* we already know that at least on one band we know the country IE, so we scan for our SSID */
        pSme->tScanParams.tDesiredSsid[ 0 ].eVisability = SCAN_SSID_VISABILITY_HIDDEN;
        os_memoryCopy (pSme->hOS, &(pSme->tScanParams.tDesiredSsid[ 0 ].tSsid), &(pSme->tSsid), sizeof (TSsid));
        /* 
         * if, in addition, we scan the other band to find its country, and the desired SSDI is not any SSID, 
         * add an empty SSID
         */
        if ((SSID_TYPE_ANY != pSme->eSsidType) &&
            (((TI_TRUE == bBandChannelExist[ RADIO_BAND_2_4_GHZ ]) && (TI_FALSE == bBandCountryFound[ RADIO_BAND_2_4_GHZ ])) ||
             ((TI_TRUE == bBandChannelExist[ RADIO_BAND_5_0_GHZ ]) && (TI_FALSE == bBandCountryFound[ RADIO_BAND_5_0_GHZ ]))))
        {
            pSme->tScanParams.tDesiredSsid[ 1 ].eVisability = SCAN_SSID_VISABILITY_PUBLIC;
            pSme->tScanParams.tDesiredSsid[ 1 ].tSsid.len = 0;
            pSme->tScanParams.uSsidNum = 2;
            pSme->tScanParams.uSsidListFilterEnabled = 1;
            /* 
             * since we are also looking for an AP with country IE (not include in IBSS), we need to make sure
             * the desired BSS type include infrastructure BSSes.
             */
            if (BSS_INDEPENDENT == pSme->eBssType)
            {
                /* the desired is only IBSS - scan for any */
                pSme->tScanParams.eBssType = BSS_ANY;
            }
            else
            {
                /* the desired is either infrastructure or any - use it */
                pSme->tScanParams.eBssType = pSme->eBssType;
            }
        }
        else
        {
            pSme->tScanParams.uSsidNum = 1;
            pSme->tScanParams.uSsidListFilterEnabled = 1;
            /* only looking for the desired SSID - set the desired BSS type */
            pSme->tScanParams.eBssType = pSme->eBssType;
        }
    }
    /* no scanned band has a counrty code - meaning all scan is passive (to find country) */
    else
    {
        TRACE0(pSme->hReport, REPORT_SEVERITY_INFORMATION , "sme_StartScan: performing passive scan to find country IE\n");
        pSme->tScanParams.eBssType = BSS_INFRASTRUCTURE; /* only an AP would transmit a country IE */
        pSme->tScanParams.uSsidNum = 0;
        pSme->tScanParams.uSsidListFilterEnabled = 1;
    }

    /* update scan cycle number and scan intervals according to 802.11d status and country availability  */
    sme_updateScanCycles (hSme, bDEnabled, bCountryValid, pSme->bConstantScan);

    /* Finally(!!!), start the scan */
    tStatus = scanCncn_StartPeriodicScan (pSme->hScanCncn, SCAN_SCC_DRIVER, &(pSme->tScanParams));
    if (SCAN_CRS_SCAN_RUNNING != tStatus)
    {
        TRACE1(pSme->hReport, REPORT_SEVERITY_ERROR , "sme_StartScan: scan concentrator returned status %d\n", tStatus);
        return TI_NOK;
    }

    return TI_OK;
}

/** 
 * \fn     sme_updateScanCycles
 * \brief  Updates the scan intervals and cycle number according to 802.11d status, country availability and WSC PB mode
 * 
 * Updates the scan intervals and cycle number according to 802.11d status, country availability and WSC PB mode.
 * Possible scenarios - D disabled - WSC PB off - scan forever with supplied intervals
 *                    - D enabled - country unknown - WSC PB off - scan forever with supplied intervals
 *                    - D disabled - WSC PB on - scan for two minutes with zero intervals
 *                    - D enabled - country unknown - WSC PB on - scan for two minutes with zero intervals
 *                    - D enabled - country known - WSC PB off - scan until country expiry with supplied intervals
 *                    - D enabled - country known - WSC PB on - scan for the minimu of two minutes and country expiry with zero intervals
 * 
 * \param  hSme - handle to the SME object
 * \param  bDEnabled - TRUE if 802.11d is enabled
 * \param  bCountryValid - TRUE if a country IE is valid for a band on which we scan
 * \param  bConstantScan - TRUE if WSC PB mode is on
 * \return None
 * \sa     sme_CalculateCyclesNumber, sme_StartScan
 */ 
void sme_updateScanCycles (TI_HANDLE hSme,
                           TI_BOOL bDEnabled,
                           TI_BOOL bCountryValid,
                           TI_BOOL bConstantScan)
{
    TSme            *pSme = (TSme*)hSme;
    TI_UINT32       uIndex, uScanPeriodMs, uScanDurationMs;
    paramInfo_t     *pParam;

    /* 802.11d is disabled, or no country is valid */
    if ((TI_FALSE == bDEnabled) || (TI_FALSE == bCountryValid))
    {
        /* WSC PB mode is disabled */
        if (TI_FALSE == bConstantScan)
        {
            /* 
             * copy intervals
             * In order to avoid tight loop of scan-select or scan-select-connecting operation, 
             * the prepare scan function takes into account the value of the scan_count when setting the 16 periods in the scan command
             */
            os_memoryCopy (pSme->hOS, &(pSme->tScanParams.uCycleIntervalMsec[ 0 ]),
                           &(pSme->tInitParams.uScanIntervals[ pSme->uScanCount ]), sizeof (TI_UINT32) * (PERIODIC_SCAN_MAX_INTERVAL_NUM - pSme->uScanCount));

            for(uIndex = (PERIODIC_SCAN_MAX_INTERVAL_NUM - pSme->uScanCount); uIndex < PERIODIC_SCAN_MAX_INTERVAL_NUM; uIndex++)
            {
                pSme->tScanParams.uCycleIntervalMsec[ uIndex ] = pSme->tInitParams.uScanIntervals[ PERIODIC_SCAN_MAX_INTERVAL_NUM - 1 ];
            }

            /* scan for default number (until a result is found) */
            pSme->tScanParams.uCycleNum = pSme->tInitParams.uCycleNum;
        }
        /* WSC PB mode is enabled */
        else
        {

            /* nullify all intervals */
            os_memoryZero (pSme->hOS, &(pSme->tScanParams.uCycleIntervalMsec[ 0 ]),
                           sizeof (TI_UINT32) * PERIODIC_SCAN_MAX_INTERVAL_NUM);

            /* calculate the duration of one scan cycle */
            uScanDurationMs = 0;
            for (uIndex = 0; uIndex < pSme->tScanParams.uChannelNum; uIndex++)
            {
                uScanDurationMs += pSme->tScanParams.tChannels[ uIndex ].uMaxDwellTimeMs;
            }

            /* set the number of cycles - 2 minutes divided by one cycle duration */
            pSme->tScanParams.uCycleNum = (120000 / uScanDurationMs) + 1;
        }
    }
    /* 802.11d is enabled, and country is valid on at least one band */
    else
    {
        pParam = (paramInfo_t *)os_memoryAlloc(pSme->hOS, sizeof(paramInfo_t));
        if (!pParam)
        {
            return;
        }

        /* get country expiry time */
        pParam->paramType = REGULATORY_DOMAIN_TIME_TO_COUNTRY_EXPIRY;
        regulatoryDomain_getParam (pSme->hRegDomain, pParam);

        /* WSC PB mode is disabled */
        if (TI_FALSE == bConstantScan)
        {
            /* 
             * copy intervals
             * In order to avoid tight loop of scan-select or scan-select-connecting operation, 
             * the prepare scan function takes into account the value of the scan_count when setting the 16 periods in the scan command
             */
            os_memoryCopy (pSme->hOS, &(pSme->tScanParams.uCycleIntervalMsec[ 0 ]),
                           &(pSme->tInitParams.uScanIntervals[ pSme->uScanCount ]), sizeof (TI_UINT32) * (PERIODIC_SCAN_MAX_INTERVAL_NUM - pSme->uScanCount));

            for(uIndex = (PERIODIC_SCAN_MAX_INTERVAL_NUM - pSme->uScanCount); uIndex < PERIODIC_SCAN_MAX_INTERVAL_NUM; uIndex++)
            {
                pSme->tScanParams.uCycleIntervalMsec[ uIndex ] = pSme->tInitParams.uScanIntervals[ PERIODIC_SCAN_MAX_INTERVAL_NUM - 1 ];
            }
 
            /* set cycle number according to country expiry time */
            sme_CalculateCyclesNumber (hSme, pParam->content.uTimeToCountryExpiryMs);
        }
        /* WSC PB mode is enabled */
        else
        {
            /* turn off WSC PB mode (for next scan) */
            pSme->bConstantScan = TI_FALSE;

            /* set scan period to minimum of WSC PB duration (2 minutes) and country expiry time */
            uScanPeriodMs = TI_MIN (120000, pParam->content.uTimeToCountryExpiryMs);

            /* nullify all intervals */
            os_memoryZero (pSme->hOS, &(pSme->tScanParams.uCycleIntervalMsec[ 0 ]),
                           sizeof (TI_UINT32) * PERIODIC_SCAN_MAX_INTERVAL_NUM);

            /* calculate the duration of one scan cycle */
            uScanDurationMs = 0;
            for (uIndex = 0; uIndex < pSme->tScanParams.uChannelNum; uIndex++)
            {
                uScanDurationMs += pSme->tScanParams.tChannels[ uIndex ].uMaxDwellTimeMs;
            }

            if (uScanDurationMs != 0)
            {
                /* set the number of cycles - scan period divided by one cycle duration */
                pSme->tScanParams.uCycleNum = (uScanPeriodMs / uScanDurationMs) + 1;
            }
            else
            {
                pSme->tScanParams.uCycleNum = pSme->tInitParams.uCycleNum;
            }
        }
        os_memoryFree(pSme->hOS, pParam, sizeof(paramInfo_t));
    }

    /* in case independent mode and to avoid supplicant send disconnect event after 60s */
    if (pSme->eBssType != BSS_INFRASTRUCTURE)
    {
        pSme->tScanParams.uCycleNum = 1;
    }
}

/** 
 * \fn     sme_CalculateCyclesNumber
 * \brief  Calculates the cycle number required for a gicen time, according to scan intervals
 * 
 * Calculates the cycle number required for a gicen time, according to scan intervals. First check the 16
 * different intervals, and if more time is available, find how many cycles still fit. Write the result
 * to the SME scan command
 * 
 * \param  hSme - handle to the SME object
 * \param  uToTalTimeMs - the total periodic scan operation duartion
 * \return None
 * \sa     sme_updateScanCycles, sme_StartScan
 */ 
void sme_CalculateCyclesNumber (TI_HANDLE hSme, TI_UINT32 uTotalTimeMs)
{
    TSme            *pSme = (TSme*)hSme;
    TI_UINT32       uIndex, uCurrentTimeMs = 0;

    /* 
     * the total time should exceed country code expiration by one interval (so that next scan wouldn't
     * have a valid country code)
     */

    /* nullify cycle number */
    pSme->tScanParams.uCycleNum = 0;
    /* now find how many cycles fit within this time. First, check if all first 16 configured intervals fit */
    for (uIndex = 0; 
         (uIndex < PERIODIC_SCAN_MAX_INTERVAL_NUM) && (uCurrentTimeMs < uTotalTimeMs);
         uIndex++)
    {
        pSme->tScanParams.uCycleNum++;
        uCurrentTimeMs += pSme->tScanParams.uCycleIntervalMsec[ uIndex ];
    }
    /* now find out how many more cycles with the last interval still fits */
    if (uCurrentTimeMs < uTotalTimeMs)
    {
        /* 
         * divide the reamining time (time until expiry minus the total time calculated so far)
         * by the last interval time, to get how many more scans would fit after the first 16 intervals
         */
        pSme->tScanParams.uCycleNum += (uTotalTimeMs - uCurrentTimeMs) / 
                                            pSme->tScanParams.uCycleIntervalMsec[ PERIODIC_SCAN_MAX_INTERVAL_NUM - 1];
        /* and add one, to compensate for the reminder */
        pSme->tScanParams.uCycleNum++;
    }
}

