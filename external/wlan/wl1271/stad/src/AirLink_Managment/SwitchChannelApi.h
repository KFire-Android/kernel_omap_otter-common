/*
 * SwitchChannelApi.h
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

/** \file SwitchChannel.h
 *  \brief SwitchChannel module interface header file
 *
 *  \see SwitchChannel.c
 */

/***************************************************************************/
/*																			*/
/*	  MODULE:	SwitchChannel.h														*/
/*    PURPOSE:	SwitchChannel module inteerface header file							*/
/*																			*/
/***************************************************************************/
#ifndef __SWITCH_CHANNEL_H__
#define __SWITCH_CHANNEL_H__

#include "paramOut.h"
#include "DrvMainModules.h"


TI_HANDLE switchChannel_create(TI_HANDLE hOs);

void      switchChannel_init (TStadHandlesList *pStadHandles);

TI_STATUS switchChannel_SetDefaults (TI_HANDLE hSwitchChannel, SwitchChannelInitParams_t *SwitchChannelInitParams);

TI_STATUS switchChannel_stop(TI_HANDLE hSwitchChannel);

TI_STATUS switchChannel_start(TI_HANDLE hSwitchChannel);

TI_STATUS switchChannel_unload(TI_HANDLE hSwitchChannel);

void switchChannel_recvCmd(TI_HANDLE hSwitchChannel, dot11_CHANNEL_SWITCH_t *channelSwitch, TI_UINT8 channel);

void switchChannel_enableDisableSpectrumMngmt(TI_HANDLE hSwitchChannel, TI_BOOL enableDisable);



/* for debug */
typedef enum
{
	SC_SWITCH_CHANNEL_NUM,
	SC_SWITCH_CHANNEL_TBTT,
	SC_SWITCH_CHANNEL_MODE
} SC_switchChannelCmdParam_e;

#ifdef TI_DBG

void switchChannelDebug_switchChannelCmdTest(TI_HANDLE hSwitchChannel, TI_BOOL BeaconPacket);
void switchChannelDebug_CancelSwitchChannelCmdTest(TI_HANDLE hSwitchChannel, TI_BOOL BeaconPacket);
void switchChannelDebug_setChannelValidity(TI_HANDLE hSwitchChannel, TI_UINT8 channelNum, TI_BOOL validity);
void switchChannelDebug_printStatus(TI_HANDLE hSwitchChannel);
void switchChannelDebug_setCmdParams(TI_HANDLE hSwitchChannel, SC_switchChannelCmdParam_e switchChannelCmdParam, TI_UINT8 param);
void switchChannelDebug_SwitchChannelCmdTest(TI_HANDLE hSwitchChannel, TI_BOOL BeaconPacket);
#endif

#endif /* __SWITCH_CHANNEL_H__*/
