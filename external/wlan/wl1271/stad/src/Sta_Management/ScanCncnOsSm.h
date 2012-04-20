/*
 * ScanCncnOsSm.h
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

/** \file  ScanCncnOsSm.h
 *  \brief Scan concentartor OS state-machine public definitions
 *
 *  \see   ScanCncnOsSm.c
 */

#ifndef __SCAN_CNCN_OS_SM_H__
#define __SCAN_CNCN_OS_SM_H__

#include "osTIType.h"

typedef enum
{
    SCAN_CNCN_OS_SM_STATE_IDLE = 0,
    SCAN_CNCN_OS_SM_STATE_SCAN_ON_G,
    SCAN_CNCN_OS_SM_STATE_SCAN_ON_A,
    SCAN_CNCN_OS_SM_NUMBER_OF_STATES
} EScanCncnOsSmStates;

typedef enum
{
    SCAN_CNCN_OS_SM_EVENT_START_SCAN = 0,
    SCAN_CNCN_OS_SM_EVENT_SCAN_COMPLETE,
    SCAN_CNCN_OS_SM_NUMBER_OF_EVENTS
} EScanCncnOsSmEvents;

TI_HANDLE   scanCncnOsSm_Create (TI_HANDLE hScanCncn);
void        scanCncnOsSm_Init (TI_HANDLE hScanCncn);
void        scanCncnOsSm_Destroy (TI_HANDLE hScanCncn);

#endif /* __SCAN_CNCN_OS_SM_H__ */

