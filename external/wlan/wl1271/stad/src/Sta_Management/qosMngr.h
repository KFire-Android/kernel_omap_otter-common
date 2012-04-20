/*
 * qosMngr.h
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


/** \file qosMngr.h
 *  \brief QOS manager module internal header file
 *
 *  \see qosMngr.c
 */

/***************************************************************************/
/*                                                                         */
/*    MODULE:   qosMgr.h                                                   */
/*    PURPOSE:  QOS manager module internal header file                    */
/*                                                                         */
/***************************************************************************/
#ifndef __QOS_MNGR_H__
#define __QOS_MNGR_H__

#include "paramOut.h"
#include "trafficAdmControl.h"

/*
 *          Defines
 */

#define QOS_MNGR_INIT_BIT_LOCAL_VECTOR     (0x01)
#define QOS_MNGR_INIT_BIT_ADM_CTRL     (0x02)

#define AC_PARAMS_AIFSN_MASK               (0x0f)
#define AC_PARAMS_ACI_MASK                 (0x60)
#define AC_PARAMS_CWMIN_MASK               (0x0f)
#define AC_PARAMS_CWMAX_MASK               (0xf0)
#define AC_PARAMS_ACM_MASK                 (0x10)

#define MAX_ENABLED_PS_RX_STREAMS          4

#if 0
#define PS_PARAMETERS_LEGACY                    (0)
#define PS_PARAMETERS_UPSD_TRIGGER_ENABLE_MASK  (0x01)
#define PS_PARAMETERS_UPSD_DELIVERY_ENABLE_MASK (0x02)
#define PS_UPSD_TRIGER_AND_DELIVERY (PS_PARAMETERS_UPSD_TRIGGER_ENABLE_MASK | PS_PARAMETERS_UPSD_DELIVERY_ENABLE_MASK)

#define CONVERT_DRIVER_PS_MODE_TO_FW(PsMode_e)  ((PsMode_e == UPSD) ? PS_UPSD_TRIGER_AND_DELIVERY : PS_PARAMETERS_LEGACY)
#endif


/*
 *          Enumerations                                                    
 */
#define RX_QUEUE_WIN_SIZE       8

typedef enum
{   
    BA_POLICY_DISABLE                                   =   0,
    BA_POLICY_INITIATOR                                 =   1, 
    BA_POLICY_RECEIVER                                  =   2,
    BA_POLICY_INITIATOR_AND_RECEIVER                    =   3
} EqosMngrBaPolicy;


/*
 *          Structures                                                      
 */

typedef struct 
{
    tspecInfo_t     currentTspecInfo[MAX_NUM_OF_AC];
    tspecInfo_t     candidateTspecInfo[MAX_NUM_OF_AC];
    TI_UINT16       totalAllocatedMediumTime;
}resourceMgmt_t;

/*
 * per AC parameters
 */
typedef struct
{
    TQueueTrafficParams   QtrafficParams;    /* AC traffic confogiration params */
    TQueueTrafficParams   QTrafficInitParams;/* for disconnect - defaults traffic params */
    TAcQosParams          acQosParams;              
    TAcQosParams          acQosInitParams;
    AckPolicy_e           wmeAcAckPolicy;     /* ack policy per AC               */
    PSScheme_e            currentWmeAcPsMode; /* current wme per ac power save mode */
    PSScheme_e            desiredWmeAcPsMode; /* desired wme per ac power save mode */
    EAdmissionState       apInitAdmissionState; /* AC admission state              */
    TI_UINT32             msduLifeTimeParam;    
}acParams_t;


typedef TI_STATUS (*qosMngrCallb_t) (TI_HANDLE hApConn, trafficAdmRequestStatus_e result);

/*
 *  qosMngr handle 
 */

typedef struct
{
    TI_HANDLE           hSiteMgr;
    TI_HANDLE           hTWD;
    TI_HANDLE           hTxCtrl;
    TI_HANDLE           hTxMgmtQ;
    TI_HANDLE           hEvHandler;
    TI_HANDLE           hRoamMng;

    TI_HANDLE           hMeasurementMngr;
    TI_HANDLE           hSmeSm;
    TI_HANDLE           hCtrlData;
    TI_HANDLE           hXCCMgr;

    TI_HANDLE           hReport;
    TI_HANDLE           hOs;
    TI_HANDLE           hTimer;
    TI_HANDLE           hStaCap;

    TI_BOOL             WMEEnable;                           /* driver supports WME protocol       */
    TI_BOOL             WMESiteSupport;                      /* site support WME protocol          */
    EQosProtocol        activeProtocol;                      /* active protocol: XCC,WME or none.  */
    TI_BOOL             tagZeroConverHeader;                 /* converting tag zero headers        */
        
    TI_UINT8            qosPacketBurstEnable;                /* Packet Burst is Enable or NOT      */
    TI_UINT32           qosPacketBurstTxOpLimit;             /* TxOp limit in case of NON_QOS */
                                                             /* protocol and Packet Burst is Enable */

    acParams_t          acParams[MAX_NUM_OF_AC];             /* per ac parameters                  */

    TI_BOOL             isConnected;                         /* Connected or not ?                       */
    PSScheme_e          desiredPsMode;                       /* The desired PS mode of the station */
    PSScheme_e          currentPsMode;                       /* The current PS mode of the station */
    TI_UINT8            ApQosCapabilityParameters;
    TI_UINT8            desiredMaxSpLen;

    EHeaderConvertMode  headerConvetMode;
    TRxTimeOut          rxTimeOut;

    /* PS Rx streaming parameters */
    TPsRxStreaming      aTidPsRxStreaming[MAX_NUM_OF_802_1d_TAGS];/* Per TID PS-Rx-Streaming configured parameters */
    TI_UINT32           uNumEnabledPsRxStreams;             /* the number of enabled TID-PS-Rx-Streams */

    /* traffic admission control parameters */
    TI_BOOL             trafficAdmCtrlEnable;                /* driver supports Admission control  */   
    trafficAdmCtrl_t    *pTrafficAdmCtrl;                    /* adm ctrl object */
    resourceMgmt_t      resourceMgmtTable;
    TI_UINT8            QosNullDataTemplateUserPriority;     /* Holds the last User Priority set into the firmware in the QOS Null data template */

    TI_BOOL             performTSPECRenegotiation;
    TI_BOOL             voiceTspecConfigured;
    TI_BOOL		        videoTspecConfigured;
    TI_HANDLE           TSPECNegotiationResultModule;
    qosMngrCallb_t      TSPECNegotiationResultCallb;
    OS_802_11_QOS_TSPEC_PARAMS  tspecRenegotiationParams[MAX_NUM_OF_AC];

    TI_BOOL             bCwFromUserEnable;
    TI_UINT8            uDesireCwMin;		/**< The contention window minimum size (in slots) from ini file */
    TI_UINT16           uDesireCwMax;		/**< The contention window maximum size (in slots) from ini file */

    /* 802.11n BA session */
    TI_UINT8               aBaPolicy[MAX_NUM_OF_802_1d_TAGS];
    TI_UINT16              aBaInactivityTimeout[MAX_NUM_OF_802_1d_TAGS];
	TI_BOOL				bEnableBurstMode;
} qosMngr_t;


#endif /* QOS_MNGR_H */
