/*
 * currBssApi.h
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

/** \file currBssApi.h
 *  \brief Current BSS module API
 *
 *  \see currBss.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Current BSS                                               *
 *   PURPOSE: Current BSS Module API                                    *
 *                                                                          *
 ****************************************************************************/

#ifndef _CURR_BSS_API_H_
#define _CURR_BSS_API_H_

#include "siteMgrApi.h"
#include "roamingMngrTypes.h"
#include "paramOut.h"

/* Constants */

/* Enumerations */

/* Structures */

/**
* Current BSS control block 
* Following structure defines parameters that can be configured externally,
* internal variables, and handlers of other modules used by Current BSS module
*/

TI_STATUS currBss_registerBssLossEvent(TI_HANDLE hCurrBSS,TI_UINT32  uNumOfBeacons, TI_UINT16 uClientID);
TI_STATUS currBss_registerTxRetryEvent(TI_HANDLE hCurrBSS,TI_UINT8   uMaxTxRetryThreshold, TI_UINT16 uClientID);
TI_INT8 currBSS_RegisterTriggerEvent (TI_HANDLE hCurrBSS, TI_UINT8 triggerID,TI_UINT16 clientID, void* fCB, TI_HANDLE hCB);
TI_STATUS currBSS_setParam(TI_HANDLE hCurrBSS, paramInfo_t *pParam);
TI_STATUS currBSS_getParam(TI_HANDLE hCurrBSS, paramInfo_t *pParam);

#endif /*  _CURR_BSS_API_H_*/

