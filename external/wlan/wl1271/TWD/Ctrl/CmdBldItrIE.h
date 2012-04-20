/*
 * CmdBldItrIE.h
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


#ifndef CMDBLDITRIE_H
#define CMDBLDITRIE_H


TI_STATUS cmdBld_ItrIeMemoryMap                	(TI_HANDLE hCmdBld, MemoryMap_t *apMap, void *fCb, TI_HANDLE hCb);
TI_STATUS cmdBld_ItrIeRoamimgStatisitics       	(TI_HANDLE hCmdBld, void * fCb, TI_HANDLE hCb, void * pCb);
TI_STATUS cmdBld_ItrIeErrorCnt                 	(TI_HANDLE hCmdBld, void * fCb, TI_HANDLE hCb, void * pCb);
TI_STATUS cmdBld_ItrIeRSSI                     	(TI_HANDLE hCmdBld, void * fCb, TI_HANDLE hCb, void * pCb);
TI_STATUS cmdBld_ItrIeSg                       	(TI_HANDLE hCmdBld, void * fCb, TI_HANDLE hCb, void * pCb);
TI_STATUS cmdBld_ItrIeStatistics               	(TI_HANDLE hCmdBld, void * fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrIeDataFilterStatistics     	(TI_HANDLE hCmdBld, void * fCb, TI_HANDLE hCb, void *pCb);
TI_STATUS cmdBld_ItrIeMediumOccupancy          	(TI_HANDLE hCmdBld, TInterrogateCmdCbParams interogateCmdCBParams);
TI_STATUS cmdBld_ItrIeTfsDtim                  	(TI_HANDLE hCmdBld, TInterrogateCmdCbParams interogateCmdCBParams);
TI_STATUS cmdBld_ItrIeNoiseHistogramResults    	(TI_HANDLE hCmdBld, TInterrogateCmdCbParams noiseHistCBParams);
TI_STATUS cmdBld_ItrIePowerConsumptionstat      (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void* pCb);
TI_STATUS cmdBld_ItrIeRateParams                (TI_HANDLE hCmdBld, void *fCb, TI_HANDLE hCb, void* pCb);

#endif

