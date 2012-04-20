/*
 * keyParserExternal.h
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

/** \file keyParserExternal.h
 *  \brief WEP Key PArser Header file
 *
 *  \see keyParser.h
 */


/***************************************************************************/
/*																		   */
/*		MODULE:	keyParserExternal.h											   */
/*    PURPOSE:	External key parser											   */
/*																	 	   */
/***************************************************************************/

#ifndef _KEY_PARSER_EXTERNAL_H
#define _KEY_PARSER_EXTERNAL_H

#include "fsm.h"
#include "keyParser.h"

/* Constants */
#define MAX_EXT_KEY_DATA_LENGTH   32


#define EXT_KEY_TRANSMIT_MASK				0x80000000		/*< bit 31 of key index field */
#define EXT_KEY_PAIRWISE_GROUP_MASK			0x40000000		/*< bit 30 of key index field */
#define EXT_KEY_RSC_KEY_MASK				0x20000000		/*< bit 29 of key index field */
#define EXT_KEY_SUPP_AUTHENTICATOR_MASK		0x10000000		/*< bit 28 of key index field */
#define EXT_KEY_REMAIN_BITS_MASK			0x0FFFFF00		/*< remaining 20 bits in external key index */

#define WEP_KEY_LEN_40	 5  /* 40 bit (5 byte) key */
#define WEP_KEY_LEN_104	 13 /* 104 bit (13 byte) key */
#define WEP_KEY_LEN_232	 29 /* 232 bit (29 byte) key */

                                                      
/* Enumerations */                                    
                                                      
/* Typedefs */                                        
               



typedef struct
{
	TI_UINT32		Length;						    		/* The length of the whole structure */
	TI_UINT32		KeyIndex;								/* The key index to add or remove */
	TI_UINT8		macAddress[MAC_ADDR_LEN];	/**< Remote station MAC address*/
} keyParserExternal_keyRemoveDesc_t;

/* External data definitions */

/* External functions definitions */

/* Function prototypes */

TI_STATUS keyParserExternal_config(struct _keyParser_t *pKeyParser);

TI_STATUS keyParserExternal_recv(struct _keyParser_t *pKeyParser,
						  TI_UINT8 *pPacket, TI_UINT32 packetLen);

TI_STATUS keyParserExternal_remove(struct _keyParser_t *pKeyParser, TI_UINT8 *pKeyData, TI_UINT32 keyDataLen);

#endif /*_KEY_PARSER_EXTERNAL_H*/

