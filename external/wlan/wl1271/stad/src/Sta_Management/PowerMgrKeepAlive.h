/*
 * PowerMgrKeepAlive.h
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


/** 
 * \file  PowerMgrKeepAlive.h
 * \brief user keep-alive declarations
 */

#ifndef __POWER_MGR_KEEP_ALIVE_H__
#define __POWER_MGR_KEEP_ALIVE_H__

TI_HANDLE powerMgrKL_create (TI_HANDLE hOS);
void powerMgrKL_destroy (TI_HANDLE hPowerMgrKL);
void powerMgrKL_init (TI_HANDLE hPowerMgrKL, TStadHandlesList *pStadHandles);
void powerMgrKL_setDefaults (TI_HANDLE hPowerMgrKL);
TI_STATUS powerMgrKL_start (TI_HANDLE hPowerMgrKL);
TI_STATUS powerMgrKL_stop (TI_HANDLE hPowerMgrKL, TI_BOOL bDisconnect);
TI_STATUS powerMgrKL_setParam (TI_HANDLE hPowerMgrKL, paramInfo_t *pParam);
TI_STATUS powerMgrKL_getParam (TI_HANDLE hPowerMgrKL, paramInfo_t *pParam);

#endif /* __POWER_MGR_KEEP_ALIVE_H__ */

