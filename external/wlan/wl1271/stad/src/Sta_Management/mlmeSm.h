/*
 * mlmeSm.h
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

 /** \file mlmeSm.h
 *  \brief 802.11 MLME SM
 *
 *  \see mlmeSm.c
 */


/***************************************************************************/
/*                                                                         */
/*      MODULE: mlmeSm.h                                                   */
/*    PURPOSE:  802.11 MLME SM                                     */
/*                                                                         */
/***************************************************************************/

#ifndef _MLME_SM_H
#define _MLME_SM_H

#include "fsm.h"
#include "mlmeApi.h"

/* Constants */

/* changed to fit double buffer size - S.G */
/*#define MAX_MANAGEMENT_FRAME_BODY_LEN 2312*/
#define MAX_MANAGEMENT_FRAME_BODY_LEN   1476

/* Enumerations */

/* state machine states */
typedef enum 
{
    MLME_SM_STATE_IDLE          = 0,
    MLME_SM_STATE_AUTH_WAIT     = 1,
    MLME_SM_STATE_ASSOC_WAIT    = 2,
    MLME_SM_STATE_ASSOC         = 3,
    MLME_SM_NUM_STATES
} mlme_smStates_t;

/* State machine inputs */
typedef enum 
{
    MLME_SM_EVENT_START             = 0,
    MLME_SM_EVENT_STOP              = 1,
    MLME_SM_EVENT_AUTH_SUCCESS      = 2,
    MLME_SM_EVENT_AUTH_FAIL         = 3,
    MLME_SM_EVENT_ASSOC_SUCCESS     = 4,
    MLME_SM_EVENT_ASSOC_FAIL        = 5,
    MLME_SM_NUM_EVENTS          
} mlme_smEvents_t;



/* Typedefs */

typedef struct
{
    mgmtStatus_e mgmtStatus;
    TI_UINT16        uStatusCode;
} mlmeData_t;


typedef struct
{
    fsm_stateMachine_t  *pMlmeSm;
    TI_UINT8            currentState;
    mlmeData_t          mlmeData;
    legacyAuthType_e    legacyAuthType;
    TI_BOOL             reAssoc;
    DisconnectType_e    disConnType;
    mgmtStatus_e        disConnReason;
    TI_BOOL             bParseBeaconWSC;

    /* temporary frame info */
    mlmeIEParsingParams_t tempFrameInfo;
    
    /* debug info - start */
    TI_UINT32           debug_lastProbeRspTSFTime;
    TI_UINT32           debug_lastDtimBcnTSFTime;
    TI_UINT32           debug_lastBeaconTSFTime;
    TI_BOOL             debug_isFunctionFirstTime;
    TI_UINT32           totalMissingBeaconsCounter;
    TI_UINT32           totalRcvdBeaconsCounter;
    TI_UINT32           maxMissingBeaconSequence;
    TI_UINT32           BeaconsCounterPS;
    /* debug info - end */

    TI_HANDLE           hAuth;
    TI_HANDLE           hAssoc;
    TI_HANDLE           hSiteMgr;
    TI_HANDLE           hCtrlData;
    TI_HANDLE           hConn;
    TI_HANDLE           hTxMgmtQ;
    TI_HANDLE           hMeasurementMgr;
    TI_HANDLE           hSwitchChannel;
    TI_HANDLE           hRegulatoryDomain;
    TI_HANDLE           hReport;
    TI_HANDLE           hOs;
    TI_HANDLE           hCurrBss;
    TI_HANDLE           hApConn;
    TI_HANDLE           hScanCncn;
    TI_HANDLE           hQosMngr;
    TI_HANDLE           hTWD;
    TI_HANDLE           hTxCtrl;
} mlme_t;

/* Structures */

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

void mlme_smTimeout(TI_HANDLE hMlme);

/* local functions */

TI_STATUS mlme_smEvent(TI_UINT8 *currentState, TI_UINT8 event, TI_HANDLE hMlme);

/* state machine functions */
TI_STATUS mlme_smStartIdle(mlme_t *pMlme);
TI_STATUS mlme_smAuthSuccessAuthWait(mlme_t *pMlme);
TI_STATUS mlme_smAuthFailAuthWait(mlme_t *pMlme);
TI_STATUS mlme_smStopAssocWait(mlme_t *pMlme);
TI_STATUS mlme_smAssocSuccessAssocWait(mlme_t *pMlme);
TI_STATUS mlme_smAssocFailAssocWait(mlme_t *pMlme);
TI_STATUS mlme_smStopAssoc(mlme_t *pMlme);
TI_STATUS mlme_smActionUnexpected(mlme_t *pMlme);
TI_STATUS mlme_smNOP(mlme_t *pMlme);

TI_STATUS mlme_smReportStatus(mlme_t *pMlme);

#endif

