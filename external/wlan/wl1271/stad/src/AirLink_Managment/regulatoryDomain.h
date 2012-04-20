/*
 * regulatoryDomain.h
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

/** \file regulatoryDomain.h
 *  \brief regulatoryDomain module internal header file
 *
 *  \see regulatoryDomain.c
 */

/***************************************************************************/
/*                                                                          */
/*    MODULE:   regulatoryDomain.h                                          */
/*    PURPOSE:  regulatoryDomain module internal header file                */
/*                                                                          */
/***************************************************************************/
#ifndef __REGULATORY_DOMAIN_H__
#define __REGULATORY_DOMAIN_H__

#include "paramOut.h"
#include "fsm.h"
#include "802_11Defs.h"

#define BG_24G_BAND_CHANNEL_HOPS    1
#define BG_24G_BAND_MIN_CHANNEL     1

#define A_5G_BAND_CHANNEL_HOPS      4

#define A_5G_BAND_MIN_MIDDLE_BAND_DFS_CHANNEL   52
#define A_5G_BAND_MAX_MIDDLE_BAND_DFS_CHANNEL   64
#define A_5G_BAND_MIN_UPPER_BAND_DFS_CHANNEL    100
#define A_5G_BAND_MAX_UPPER_BAND_DFS_CHANNEL    140


typedef struct
{
    TI_BOOL    channelValidityPassive; /*TI_TRUE-valid, TI_FALSE-invalid */
    TI_BOOL    channelValidityActive; /*TI_TRUE-valid, TI_FALSE-invalid */
    TI_BOOL    bChanneInCountryIe;

    TI_UINT8   uMaxTxPowerDomain;     /* 
									  * Holds ONLY the default limitation (Application) 
									  * or according to 11d country code IE	
									  * Updated on init phase or upon receiving new country code IE				  
									  */ 
    TI_UINT32  timestamp;
}   channelCapability_t;


typedef struct 
{
    /* Variables read from registry */
    /********************************/   
    /* 802.11h enabled or disabled */
    TI_BOOL                            	spectrumManagementEnabled;
    /* 802.11d enabled or disabled */
    TI_BOOL                            	regulatoryDomainEnabled;
    /* scan availability channels from registry */
    scanControlTable_t              	scanControlTable;
    /* Desired Temp Tx Power */
    TI_UINT8                           	uDesiredTemporaryTxPower; 
    /* Actual Temp Tx Power */
    TI_UINT8                           	uTemporaryTxPower;	
    /* User configuration for max Tx power */
    TI_UINT8                           	uUserMaxTxPower; 
    /* Tx Power Control adjustment flag on=TI_TRUE\off=TI_FALSE */
    TI_BOOL                             bTemporaryTxPowerEnable;

    /* Internal reg domain variables */
    /*********************************/

    /* Power Constraint IE 32 in DBM/10, valid only when 802.11h is enabled  */
    TI_UINT8                           	uPowerConstraint;    
    /* External TX Power Control in DBM/10, valid only when 802.11h is disabled */
    TI_UINT8                           	uExternTxPowerPreferred;       

    TI_UINT8                           	minDFS_channelNum;
    TI_UINT8                           	maxDFS_channelNum;

    TCountry                       		country24;   /* Detected County IE for 2.4 Ghz */
    TCountry                       		country5;    /* Detected County IE for 5 Ghz */
    TI_BOOL                            	country_2_4_WasFound;
    TI_BOOL                            	country_5_WasFound;
    TI_UINT32                          	uLastCountryReceivedTS;
    TI_UINT32                          	uTimeOutToResetCountryMs;
    channelCapability_t             	supportedChannels_band_5[A_5G_BAND_NUM_CHANNELS];
    channelCapability_t             	supportedChannels_band_2_4[NUM_OF_CHANNELS_24];

	/* set the size of the array to max of B_G & A, so that the array doesnt overflow. +3 for word alignment */
	TI_UINT8                        	pDefaultChannels[A_5G_BAND_NUM_CHANNELS+3];
    /* merge 4.02/4.03 evaluate the +3 above and adjust or hSiteMgr and below will be 
       will be unaligned accesses.  Expect it might now be +1 since 2 UINT8 variable 
       have been added in 4.03 (max and min DFS_channelNum above) */
    
    
    /* Handles to other objects */
    TI_HANDLE                       	hSiteMgr;
    TI_HANDLE                       	hTWD;
    TI_HANDLE                       	hSwitchChannel;
    TI_HANDLE                       	hReport;
    TI_HANDLE                       	hOs;


} regulatoryDomain_t;





#endif /* __REGULATORY_DOMAIN_H__*/
