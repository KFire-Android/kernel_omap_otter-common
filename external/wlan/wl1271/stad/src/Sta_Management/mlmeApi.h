/*
 * mlmeApi.h
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

/** \file mlmeApi.h
 *  \brief MLME API
 *
 *  \see mlmeSm.c
 */

/***************************************************************************/
/*                                                                          */
/*    MODULE:   mlmeApi.h                                                   */
/*    PURPOSE:  MLME API                                                    */
/*                                                                          */
/***************************************************************************/
#ifndef __MLME_API_H__
#define __MLME_API_H__

#include "tidef.h"
#include "paramOut.h"
#include "802_11Defs.h"
#include "TWDriver.h"
#include "DrvMainModules.h"

/* Constants */

/* Enumerations */

typedef enum
{
    MLME_LEGACY_TYPE_PARAM   = 0x01,
    MLME_RE_ASSOC_PARAM      = 0x02,
    MLME_TNET_WAKE_ON_PARAM  = 0x03,
    MLME_CAPABILITY_PARAM    = 0x04

} EMlmeParam;


typedef enum
{
    AUTH_LEGACY_TYPE_PARAM   = 0x01

} EAuthLegacyParam;


typedef enum
{
    MSG_BROADCAST,
    MSG_MULTICAST,
    MSG_UNICAST
} mlmeMsgDestType_t;

/* Typedefs */

/* Disassociation frame structure */
typedef struct
{
    TI_UINT16                   reason;    
}  disAssoc_t;


/* (Re)Association response frame structure */
#define ASSOC_RESP_FIXED_DATA_LEN 6
#define ASSOC_RESP_AID_MASK  0x3FFF  /* The AID is only in 14 LS bits. */

typedef struct
{
    TI_UINT16                   capabilities;      
    TI_UINT16                   status;    
    TI_UINT16                   aid;       
    dot11_RATES_t               *pRates;    
    dot11_RATES_t               *pExtRates;
    TI_UINT8                    useProtection;
    TI_BOOL                     ciscoIEPresent;
    EPreamble                   barkerPreambleMode;
    TI_UINT8                    NonErpPresent;  
    dot11_WME_PARAM_t           *WMEParams;
    dot11_RSN_t                 *pRsnIe;
    TI_UINT8                    rsnIeLen;
    dot11_QOS_CAPABILITY_IE_t   *QoSCapParameters;
	Tdot11HtCapabilitiesUnparse *pHtCapabilities;
	Tdot11HtInformationUnparse	*pHtInformation;
    TI_UINT8                    *tspecVoiceParameters;  /* dot11_WME_TSPEC_IE_t is unpacked so need to access as bytes. */
    TI_UINT8                    *tspecSignalParameters; /* dot11_WME_TSPEC_IE_t is unpacked so need to access as bytes. */
    XCCv4IEs_t                  XCCIEs[MAX_NUM_OF_AC];
}  assocRsp_t;


/* Probe response frame structure */
/* Please notice, the order of fields in the beacon must be identical to the order of 
    field in the probe response. This is because of the parsing that is done by the site manager. */
typedef struct
{
    char                        timestamp[TIME_STAMP_LEN];     
    TI_UINT16                   beaconInerval;     
    TI_UINT16                   capabilities;      
    dot11_SSID_t                *pSsid;    
    dot11_RATES_t               *pRates;
    dot11_COUNTRY_t             *country;
    dot11_POWER_CONSTRAINT_t    *powerConstraint;
    dot11_CHANNEL_SWITCH_t      *channelSwitch;
    dot11_QUIET_t               *quiet;
    dot11_TPC_REPORT_t          *TPCReport;

#ifdef XCC_MODULE_INCLUDED
    dot11_CELL_TP_t             *cellTP;
#endif

    dot11_WME_PARAM_t           *WMEParams;
    dot11_WSC_t                 *WSCParams;
    dot11_RATES_t               *pExtRates;
    TI_UINT8                    useProtection;
    EPreamble                   barkerPreambleMode;
    TI_UINT8                    NonErpPresent;  
    dot11_FH_PARAMS_t           *pFHParamsSet;     
    dot11_DS_PARAMS_t           *pDSParamsSet;     
    dot11_CF_PARAMS_t           *pCFParamsSet;     
    dot11_IBSS_PARAMS_t         *pIBSSParamsSet;
    dot11_RSN_t                 *pRsnIe;
    TI_UINT8                    rsnIeLen;
    dot11_QOS_CAPABILITY_IE_t   *QoSCapParameters;
	Tdot11HtCapabilitiesUnparse *pHtCapabilities;
	Tdot11HtInformationUnparse	*pHtInformation;
    dot11_TIM_t                 *pTIM;                  /* for beacons only */
    TI_UINT8                    *pUnknownIe;
    TI_UINT16                   unknownIeLen;
} beacon_probeRsp_t; 


/* Authentication message frame structure */
typedef struct
{
    TI_UINT16                   authAlgo;      
    TI_UINT16                   seqNum;    
    TI_UINT16                   status;    
    dot11_CHALLENGE_t           *pChallenge;       
}  authMsg_t;

/* DeAuthentication message frame structure */
typedef struct
{
    TI_UINT16                   reason;    
}  deAuth_t;

/* Action message frame structure */
typedef struct
{
    TI_UINT8                    frameType;
    TI_UINT8                    category;
    TI_UINT8                    action;
} action_t;


typedef struct 
{
    dot11MgmtSubType_e subType;

    union 
    {
        beacon_probeRsp_t iePacket;
        disAssoc_t  disAssoc;
        assocRsp_t  assocRsp;
        authMsg_t   auth;
        deAuth_t    deAuth;
        action_t    action;
    } content;

    union
    {
        mlmeMsgDestType_t   destType;
    } extesion;

} mlmeFrameInfo_t;

typedef struct 
{
    dot11_SSID_t            ssid;
    TMacAddr                bssid;
    dot11_CHALLENGE_t       challenge;
    dot11_RATES_t           rates;
    dot11_RATES_t           extRates;
    dot11_FH_PARAMS_t       fhParams;
    dot11_CF_PARAMS_t       cfParams;
    dot11_DS_PARAMS_t       dsParams;
    dot11_IBSS_PARAMS_t     ibssParams;
    dot11_COUNTRY_t         country;
    dot11_WME_PARAM_t       WMEParams;
    dot11_WSC_t             WSCParams;
    dot11_POWER_CONSTRAINT_t powerConstraint;
    dot11_CHANNEL_SWITCH_t  channelSwitch;
    dot11_QUIET_t           quiet;
    dot11_TPC_REPORT_t      TPCReport;
#ifdef XCC_MODULE_INCLUDED
    dot11_CELL_TP_t         cellTP;
#endif
    dot11_RSN_t             rsnIe[3];
    dot11_TIM_t             tim;
    dot11_QOS_CAPABILITY_IE_t   QosCapParams;
	Tdot11HtCapabilitiesUnparse tHtCapabilities;
	Tdot11HtInformationUnparse	tHtInformation;
    TI_UINT8                rxChannel;
    TI_UINT8                band;
    TI_BOOL                 myBssid;
    TI_BOOL                 myDst;
    TI_BOOL                 mySa;

    TI_BOOL                 recvChannelSwitchAnnoncIE;

	TI_UINT8                unknownIe[MAX_BEACON_BODY_LENGTH];

    mlmeFrameInfo_t         frame;
}mlmeIEParsingParams_t;

typedef void (*mlme_resultCB_t)( TI_HANDLE hObj, TMacAddr* bssid, mlmeFrameInfo_t* pFrameInfo,
                                 TRxAttr* pRxAttr, TI_UINT8* frame, TI_UINT16 frameLength );

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

/* MLME SM API */

TI_HANDLE mlme_create(TI_HANDLE hOs);

TI_STATUS mlme_unload(TI_HANDLE hMlme);

void      mlme_init (TStadHandlesList *pStadHandles);

void      mlme_SetDefaults (TI_HANDLE hMlmeSm, TMlmeInitParams *pMlmeInitParams);

TI_STATUS mlme_setParam(TI_HANDLE           hMlmeSm,
                        paramInfo_t         *pParam);

TI_STATUS mlme_getParam(TI_HANDLE           hMlmeSm, 
                        paramInfo_t         *pParam);

TI_STATUS mlme_start(TI_HANDLE hMlme);

TI_STATUS mlme_stop(TI_HANDLE hMlme, DisconnectType_e disConnType, mgmtStatus_e reason);

TI_STATUS mlme_reportAuthStatus(TI_HANDLE hMlme, TI_UINT16 status);

TI_STATUS mlme_reportAssocStatus(TI_HANDLE hMlme, TI_UINT16 status);

/* MLME parser API */

TI_STATUS mlmeParser_recv(TI_HANDLE hMlme, void *pBuffer, TRxAttr* pRxAttr);

TI_STATUS mlmeParser_parseIEs(TI_HANDLE hMlme, 
                              TI_UINT8 *pData,
                              TI_INT32 bodyDataLen,
                              mlmeIEParsingParams_t *params);
TI_BOOL mlmeParser_ParseIeBuffer (TI_HANDLE hMlme, TI_UINT8 *pIeBuffer, TI_UINT32 length, TI_UINT8 desiredIeId, TI_UINT8 **pDesiredIe, TI_UINT8 *pMatchBuffer, TI_UINT32 matchBufferLen);

#ifdef XCC_MODULE_INCLUDED
void mlmeParser_readXCCOui (TI_UINT8 *pData, 
                            TI_UINT32 dataLen, 
                            TI_UINT32 *pReadLen, 
                            XCCv4IEs_t *XCCIEs);
#endif

mlmeIEParsingParams_t *mlmeParser_getParseIEsBuffer(TI_HANDLE *hMlme);

/* Association SM API */

TI_HANDLE assoc_create(TI_HANDLE pOs);

TI_STATUS assoc_unload(TI_HANDLE pAssoc);

void      assoc_init (TStadHandlesList *pStadHandles);

TI_STATUS assoc_SetDefaults (TI_HANDLE hAssoc, assocInitParams_t *pAssocInitParams);

TI_STATUS assoc_setParam(TI_HANDLE hCtrlData, paramInfo_t   *pParam);

TI_STATUS assoc_getParam(TI_HANDLE hCtrlData, paramInfo_t   *pParam);

/* Authentication SM API */

TI_HANDLE auth_create(TI_HANDLE hOs);

TI_STATUS auth_unload(TI_HANDLE hAuth);

void      auth_init (TStadHandlesList *pStadHandles);

TI_STATUS auth_SetDefaults (TI_HANDLE hAuth, authInitParams_t *pAuthInitParams);

TI_STATUS auth_setParam(TI_HANDLE hCtrlData, paramInfo_t    *pParam);

TI_STATUS auth_getParam(TI_HANDLE hCtrlData, paramInfo_t    *pParam);

#endif /* __MLME_API_H__*/
