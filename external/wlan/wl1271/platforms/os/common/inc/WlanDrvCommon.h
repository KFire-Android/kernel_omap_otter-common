/*
 * WlanDrvCommon.h
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



/** \file   WlanDrvCommon.h 
 *  \brief  Defines WlanDrvIf objects common to all OS types.                                  
 *
 *  \see    WlanDrvIf.h
 */

#ifndef __WLAN_DRV_COMMON_H__
#define __WLAN_DRV_COMMON_H__


#include "tidef.h"
#include "TWDriver.h"

#define DRV_ADDRESS_SIZE					(sizeof(TI_INT32))
#define MAX_CHUNKS_IN_FILE					(1000)
#define OS_SPECIFIC_RAM_ALLOC_LIMIT			(0xFFFFFFFF)	/* assume OS never reach that limit */

/* Driver steady states - for driver external users */
typedef enum 
{
    DRV_STATE_IDLE,
    DRV_STATE_RUNNING,
    DRV_STATE_STOPING,
    DRV_STATE_STOPPED,
    DRV_STATE_FAILED
} EDriverSteadyState;


/* The driver Start/Stop actions */
typedef enum
{
    ACTION_TYPE_NONE, 
    ACTION_TYPE_START, 
    ACTION_TYPE_STOP
} EActionType;

/* Initialization file info */
typedef struct 
{
    void            *pImage;
    unsigned long    uSize;
} TInitImageInfo;

/* WlanDrvIf object common part (included by TWlanDrvIfObj from each OS abstraction layer) */
typedef struct 
{
    /* Other modules handles */
    void               *hDrvMain;
    void               *hCmdHndlr;
    void               *hContext;
    void               *hTxDataQ;
    void               *hTxMgmtQ;
    void               *hTxCtrl;
    void               *hTWD;
    void               *hEvHandler;
    void               *hReport;
    void               *hCmdDispatch;

    /* Initialization files info */
    TInitImageInfo      tIniFile;
    TInitImageInfo      tNvsImage;
    TInitImageInfo      tFwImage;

    EDriverSteadyState  eDriverState;   /* The driver state as presented to the OS */
    TI_UINT32           uLinkSpeed;

} TWlanDrvIfCommon;


/* The loader files interface */
typedef struct
{
  TI_UINT32 uNvsFileLength;
  TI_UINT32 uFwFileLength;
  TI_UINT32 uIniFileLength;
  char data[1];
  /* eeprom image follows   */
  /* firmware image follows */
  /* init file follows      */
} TLoaderFilesData;



#endif /* __WLAN_DRV_COMMON_H__ */
