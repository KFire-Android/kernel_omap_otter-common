/*
 * keyParser.h
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

/** \file keyParser.h
 *  \brief key parser API
 *
 *  \see keyParser.c
 */

/****************************************************************************
 *                                                                          *
 *   MODULE:  Key Parser	                                            *
 *   PURPOSE: key Parser module API                                    *
 *                                                                          *
 ****************************************************************************/

#ifndef _KEY_PARSER_H
#define _KEY_PARSER_H

#include "rsnApi.h"
#include "keyTypes.h"

/* Constants */
#define MAX_REPLAY_COUNTER_LEN		8

/* Enumerations */

/* Typedefs */

typedef struct _keyParser_t	keyParser_t;

/* RSN admission control prototypes */

typedef TI_STATUS (*keyParser_recv_t)(struct _keyParser_t *pKeyParser, TI_UINT8 *pKeyData, TI_UINT32 keyDataLen);
typedef TI_STATUS (*keyParser_config_t)(struct _keyParser_t *pKeyParser, TI_HANDLE hReport, TI_HANDLE hOs);
typedef TI_STATUS (*keyParser_replayReset_t)(struct _keyParser_t *pKeyParser);
typedef TI_STATUS (*keyParser_remove_t)(struct _keyParser_t *pKeyParser, TI_UINT8 *pKeyData, TI_UINT32 keyDataLen);

/* Structures */

struct _keyParser_t
{
	TI_UINT8					replayCounter[MAX_REPLAY_COUNTER_LEN];

	struct _mainKeys_t		*pParent;
	struct _unicastKey_t	*pUcastKey;
	struct _broadcastKey_t	*pBcastKey;

	TRsnPaeConfig         *pPaeConfig;

	TI_HANDLE 				hReport;
	TI_HANDLE 				hOs;
    TI_HANDLE               hCtrlData;

	keyParser_config_t		config;
	keyParser_recv_t		recv;
	keyParser_replayReset_t replayReset;
	keyParser_remove_t		remove;
};

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

keyParser_t* keyParser_create(TI_HANDLE hOs);

TI_STATUS keyParser_config(struct _keyParser_t *pKeyParser,
						TRsnPaeConfig *pPaeConfig,
                        struct _unicastKey_t *pUcastKey,
                        struct _broadcastKey_t *pBcastKey,
                        struct _mainKeys_t *pParent,
						TI_HANDLE hReport,
						TI_HANDLE hOs,
                        TI_HANDLE hCtrlData);

TI_STATUS keyParser_unload(struct _keyParser_t *pKeyParser);

TI_STATUS keyParser_nop(keyParser_t *pKeyParser);

#endif /*  _KEY_PARSER_H*/
