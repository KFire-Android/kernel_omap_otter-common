/*
 * tiQosTypes.h
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


/*--------------------------------------------------------------------------*/
/* Module:      tiQosTypes.h*/
/**/
/* Purpose:     */
/**/
/*--------------------------------------------------------------------------*/

#ifndef TI_QOS_TYPES_H
#define TI_QOS_TYPES_H


#define MAX_NUM_OF_802_1d_TAGS          8

/*
 * This enum defines the protocol modes of the QOS management object
 */
typedef enum
{
    QOS_WME,
    QOS_NONE

} EQosProtocol;


/*
 * This enum includes the header converting modes configured to dataCtrl object.
 */
typedef enum
{
    HDR_CONVERT_NONE,
    HDR_CONVERT_QOS,
    HDR_CONVERT_LEGACY

} EHeaderConvertMode;


typedef enum
{
    QOS_AC_BE = 0,
    QOS_AC_BK,
    QOS_AC_VI,
    QOS_AC_VO,
    QOS_HIGHEST_AC_INDEX = QOS_AC_VO

} EAcTrfcType;


#define FIRST_AC_INDEX                  QOS_AC_BE
#define AC_PARAMS_MAX_TSID              15
#define MAX_APSD_CONF                   0xffff


typedef enum
{
    DROP_NEW_PACKET = 0,
    DROP_OLD_PACKET

} EQOverflowPolicy;


typedef enum
{
    AC_NOT_ADMITTED,
    AC_WAIT_ADMISSION,
    AC_ADMITTED

} ETrafficAdmState;


/* 
 * This enum defines the admission state configured to dataCtrl object.
 */
typedef enum
{
    ADMISSION_NOT_REQUIRED,
    ADMISSION_REQUIRED

} EAdmissionState;


typedef struct
{
    /* Power save mode */
    TI_UINT8                PsMode;             
    TI_UINT16               TxQueueSize;
    TI_UINT8                QueueIndex;
    EQOverflowPolicy        QueueOvFlowPolicy;
    TI_UINT8                ackPolicy;
    TI_UINT32               MsduLifeTime;

} TAcTrfcCtrl;

typedef struct
{
    /* header converting mode */
    EHeaderConvertMode      headerConverMode;                           
    /* flag for converting zero tags */
    TI_BOOL                 convertTagZeroFrames;                       
    /* AC admission state */
    ETrafficAdmState        admissionState;                             
    /* AC admission is mandatory */
    EAdmissionState         admissionRequired;                          
    /* Tag to AC classification */
    EAcTrfcType             tag_ToAcClsfrTable[MAX_NUM_OF_802_1d_TAGS]; 

} TQosParams;


typedef struct
{
    TAcTrfcCtrl             acTrfcCtrl;
    TQosParams              qosParams;
    TI_UINT8                *tsrsArr;
    TI_UINT8                tsrsArrLen;
    TI_UINT8                acID;

} TTxDataQosParams;


typedef struct _OS_802_11_QOS_PARAMS
{
    TI_UINT32               acID;
    TI_UINT32               MaxLifeTime;
    TI_UINT32               PSDeliveryProtocol;
	TI_UINT32				VoiceDeliveryProtocol;

} OS_802_11_QOS_PARAMS;


typedef struct  
{
    TI_UINT32               psPoll;
    TI_UINT32               UPSD;

} OS_802_11_QOS_RX_TIMEOUT_PARAMS;


typedef struct _OS_802_11_AC_QOS_PARAMS
{
    TI_UINT32               uAC;
    TI_UINT32               uAssocAdmissionCtrlFlag;
    TI_UINT32               uAIFS;
    TI_UINT32               uCwMin;
    TI_UINT32               uCwMax;
    TI_UINT32               uTXOPLimit;

} OS_802_11_AC_QOS_PARAMS;


typedef struct _OS_802_11_AP_QOS_CAPABILITIES_PARAMS
{
    TI_UINT32               uQOSFlag;
    TI_UINT32               uAPSDFlag;

} OS_802_11_AP_QOS_CAPABILITIES_PARAMS;


typedef struct _OS_802_11_QOS_TSPEC_PARAMS
{
    TI_UINT32               uUserPriority;
    TI_UINT32               uNominalMSDUsize; /* in bytes */
    TI_UINT32               uMeanDataRate;        /* bits per second */
    TI_UINT32               uMinimumPHYRate;  /* 1,2,5,6,9,11,12,18,......*/
    TI_UINT32               uSurplusBandwidthAllowance;
    TI_UINT32               uAPSDFlag;
    TI_UINT32               uMediumTime;
    TI_UINT32               uReasonCode;
    TI_UINT32               uMinimumServiceInterval;
    TI_UINT32               uMaximumServiceInterval;

} OS_802_11_QOS_TSPEC_PARAMS;


typedef struct _OS_802_11_QOS_DELETE_TSPEC_PARAMS
{
    TI_UINT32               uUserPriority;
    TI_UINT32               uReasonCode;
} OS_802_11_QOS_DELETE_TSPEC_PARAMS;


typedef struct _OS_802_11_QOS_DESIRED_PS_MODE
{
    TI_UINT32               uDesiredPsMode;
    TI_UINT32               uDesiredWmeAcPsMode[QOS_HIGHEST_AC_INDEX + 1];

} OS_802_11_QOS_DESIRED_PS_MODE;


/* When this value is added to reason code in TSPEC events, it indicates a TSPEC response which was unexpected at the time */
/* For example, a TSPEC response arrives after a TSPEC timeout */
#define TSPEC_RESPONSE_UNEXPECTED      0x1000   


typedef enum
{
    ADDTS_RESPONSE_ACCEPT = 0,
    ADDTS_RESPONSE_REJECT = 3,
    ADDTS_RESPONSE_AP_PARAM_INVALID = 253,
    ADDTS_RESPONSE_TIMEOUT = 254,
    TSPEC_DELETED_BY_AP = 255

} ETspecStatus;


typedef struct _OS_802_11_AC_UPSD_STATUS_PARAMS
{
   TI_UINT32                uAC;
   TI_UINT32                uCurrentUAPSDStatus;
   TI_UINT32                pCurrentAdmissionStatus;

} OS_802_11_AC_UPSD_STATUS_PARAMS;


typedef struct _OS_802_11_THRESHOLD_CROSS_PARAMS
{
    TI_UINT32               uAC;
    TI_UINT32               uHighThreshold;
    TI_UINT32               uLowThreshold;

} OS_802_11_THRESHOLD_CROSS_PARAMS;


typedef struct OS_802_11_THRESHOLD_CROSS_INDICATION_PARAMS
{
    TI_UINT32               uAC;
    TI_UINT32               uHighOrLowThresholdFlag;  /* According to thresholdCross_e enum */
    TI_UINT32               uAboveOrBelowFlag;        /* According to thresholdCrossDirection_e enum */

} OS_802_11_THRESHOLD_CROSS_INDICATION_PARAMS;


typedef enum
{
    HIGH_THRESHOLD_CROSS,
    LOW_THRESHOLD_CROSS

} EThresholdCross;


typedef enum
{
    CROSS_ABOVE,
    CROSS_BELOW

} EThresholdCrossDirection;


typedef struct
{
   TI_UINT32                dstIpAddress;
   TI_UINT32                dstPort;
   TI_UINT32                PktTag;
   TI_UINT32                userPriority;

} TStreamTrafficProperties;


typedef enum
{
   UPLINK_DIRECTION = 0,
   DOWNLINK_DIRECTION = 1,
   RESERVED_DIRECTION = 2,
   BI_DIRECTIONAL = 3

} EStreamDirection;


/* classification algorithms: 
  0) D-tag to D-tag
  1) DSCP to D-tag
  2) Destination port number to D-tag 
  3) Destination IP&Port to D-tag
*/
typedef enum
{
    D_TAG_CLSFR    = 0,
    DSCP_CLSFR     = 1,
    PORT_CLSFR     = 2,
    IPPORT_CLSFR   = 3,
    CLSFR_MAX_TYPE = IPPORT_CLSFR

} EClsfrType;



/*************************/
/*   classifier params   */
/*************************/

/* Destination IP address and port number */
typedef struct 
{
    TI_UINT32               DstIPAddress;
    TI_UINT16               DstPortNum;

} TIpPort;

/* Classification mapping table */
typedef struct 
{
    union   
    {
        TIpPort             DstIPPort;  /* for destination IP&Port classifier*/
        TI_UINT16           DstPortNum; /* for destination Port classifier*/
        TI_UINT8            CodePoint;  /* for DSCP classifier*/
    } Dscp;

    TI_UINT8                DTag;

} TClsfrTableEntry;

/* number of classifier entries in the classification table */
#define NUM_OF_CLSFR_TABLE_ENTRIES  16

typedef struct
{
    EClsfrType              eClsfrType; /* The type of the classifier: D-tag, DSCP, Port or IP&Port */
    TI_UINT32               uNumActiveEntries; /* The number of active entries in the classification table */
    TClsfrTableEntry        ClsfrTable[NUM_OF_CLSFR_TABLE_ENTRIES]; /* Classification table - size changed from 15 to 16*/

} TClsfrParams;

typedef struct{
	TI_UINT8                       voiceTspecConfigure;
	TI_UINT8                       videoTspecConfigure;
}TSpecConfigure;

#endif /* TI_QOS_TYPES_H */

