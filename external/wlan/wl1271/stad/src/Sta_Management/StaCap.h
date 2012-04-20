/*
 * StaCap.h
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



/** \file   StaCap.h 
 *  \brief  StaCap module header file.                                  
 *
 *  \see    StaCap.c
 */

#ifndef _STA_CAP_H_
#define _STA_CAP_H_

#include "TWDriver.h"
#include "DrvMainModules.h"

/* The main StaCap module structure */
typedef struct 
{
	TI_HANDLE   hOs;
	TI_HANDLE   hReport;
    TI_HANDLE   hTWD;
    TI_HANDLE   hQosMngr;
    TI_HANDLE   hSme;

} TStaCap;	

/* The Supported MCS Set field structure acording 802.11n SPEC*/
typedef struct 
{
    TI_UINT8    aRxMscBitmask[RX_TX_MCS_BITMASK_SIZE];
	TI_UINT16   uHighestSupportedDataRate;
	TI_UINT8    uTxRxSetting;
	TI_UINT8    aReserved[3];
} TStaCapSuppMcsSet;	

/* The HT Capabilities element structure acording 802.11n SPEC*/
typedef struct 
{
    TI_UINT16	        uHtCapabilitiesInfo;
    TI_UINT8	        uAMpduParam;
    TStaCapSuppMcsSet   tSuppMcsSet;
    TI_UINT16	        uExteCapabilities;
    TI_UINT32	        uTxBfCapabilities;
    TI_UINT8	        uAselCapabilities;
} TStaCapHtCapabilities ;	


/* External Functions Prototypes */
/* ============================= */
TI_HANDLE StaCap_Create (TI_HANDLE hOs);
TI_STATUS StaCap_Destroy (TI_HANDLE hStaCap);
TI_STATUS StaCap_Init (TStadHandlesList *pStadHandles);
TI_STATUS StaCap_GetHtCapabilitiesIe (TI_HANDLE hStaCap, TI_UINT8 *pRequest, TI_UINT32 *pLen);
void StaCap_IsHtEnable (TI_HANDLE hStaCap, TI_BOOL *b11nEnable);
#endif  /* _STA_CAP_H_ */


