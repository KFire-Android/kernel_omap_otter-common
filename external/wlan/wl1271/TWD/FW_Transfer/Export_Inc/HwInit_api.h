/*
 * HwInit_api.h
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

 
/***************************************************************************/
/*                                                                         */
/*    MODULE:   HwInit_api.h                                               */
/*    PURPOSE:  HwInit module Header file                             	   */
/*                                                                         */
/***************************************************************************/
#ifndef _HW_INIT_API_H_
#define _HW_INIT_API_H_

#include "TWDriver.h"

typedef TI_STATUS (*TFinalizeCb) (TI_HANDLE);

typedef struct
{
    TI_UINT8   MacClock;
    TI_UINT8   ArmClock;
    TI_BOOL    FirmwareDebug;

} TBootAttr;


/* Callback function definition for EndOfRecovery */
typedef void (* TEndOfHwInitCb) (TI_HANDLE handle);


TI_HANDLE hwInit_Create (TI_HANDLE hOs);
TI_STATUS hwInit_Init   (TI_HANDLE hHwInit,
                         TI_HANDLE hReport, 
                         TI_HANDLE hTimer, 
                         TI_HANDLE hTWD, 
                         TI_HANDLE hFinalizeDownload, 
                         TFinalizeCb fFinalizeDownload, 
                         TEndOfHwInitCb fInitHwCb);
TI_STATUS hwInit_SetNvsImage (TI_HANDLE hHwInit, TI_UINT8 *pbuf, TI_UINT32 length);
TI_STATUS hwInit_SetFwImage (TI_HANDLE hHwInit, TFileInfo *pFileInfo);
TI_STATUS hwInit_Destroy (TI_HANDLE hHwInit);
TI_STATUS hwInit_Boot (TI_HANDLE hHwInit);
TI_STATUS hwInit_LoadFw (TI_HANDLE hHwInit);
TI_STATUS hwInit_ReadRadioParamsSm (TI_HANDLE hHwInit);
TI_STATUS hwInit_ReadRadioParams (TI_HANDLE hHwInit);
TI_STATUS hwInit_WriteIRQPolarity(TI_HANDLE hHwInit);
TI_STATUS hwInit_InitPolarity(TI_HANDLE hHwInit);


#endif /* _HW_INIT_API_H_ */
