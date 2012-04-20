/*
 * CmdInterpretWext.h
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

#ifndef _CMD_INTERPRET_WEXT_H_
#define _CMD_INTERPRET_WEXT_H_
/*  Command interpreter header file */

#include <linux/wireless.h>

typedef struct 
{
    TI_UINT8 iw_auth_wpa_version;
    TI_UINT8 iw_auth_cipher_pairwise;
    TI_UINT8 iw_auth_cipher_group;
    TI_UINT8 iw_auth_key_mgmt;
    TI_UINT8 iw_auth_80211_auth_alg;
} wext_auth_info;

typedef struct 
{
    TI_HANDLE               hOs;            /* Pointer to the adapter object */
	TI_UINT8				nickName[IW_ESSID_MAX_SIZE + 1]; 	/* Interface nickname */
	wext_auth_info			wai;			/* Authentication parameters */
	struct iw_statistics 	wstats;			/* Statistics information */
    TI_HANDLE               hEvHandler;     /* Event-handler module handle */
	TI_HANDLE				hCmdHndlr;		/* Handle to the Command-Handler */
	TI_HANDLE				hCmdDispatch;	/* Handle to the Command-Dispatcher */
	TI_HANDLE				hEvents[IPC_EVENT_MAX];	/* Contains handlers of events registered to */
    TConfigCommand         *pAsyncCmd;       /* Pointer to the command currently being processed */
    void                   *pAllocatedBuffer;
    TI_UINT32              AllocatedBufferSize;
} cmdInterpret_t;

#define WLAN_PROTOCOL_NAME    "IEEE 802.11ABG"

typedef enum _TIWLAN_KEY_FLAGS
{
	TIWLAN_KEY_FLAGS_TRANSMIT		= 0x80000000,           /* Used whenever key should be immidiately used for TX */
	TIWLAN_KEY_FLAGS_PAIRWISE		= 0x40000000,           /* Used to indicate pairwise key */
	TIWLAN_KEY_FLAGS_SET_KEY_RSC	= 0x20000000,           /* Used to set RSC (receive sequence counter) to driver */
	TIWLAN_KEY_FLAGS_AUTHENTICATOR	= 0x10000000            /* Not used currently */
} TIWLAN_KEY_FLAGS;

#define TKIP_KEY_LENGTH		32
#define AES_KEY_LENGTH      16
#define WEP_KEY_LENGTH_40   5
#define WEP_KEY_LENGTH_104   13

#define MAX_THROUGHPUT 5500000

#define WEXT_OK					0
#define WEXT_NOT_SUPPORTED		-EOPNOTSUPP
#define WEXT_INVALID_PARAMETER  -EINVAL

#endif /* _CMD_INTERPRET_WEXT_H_ */
