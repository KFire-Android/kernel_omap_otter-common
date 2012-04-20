/*
 * keyTypes.h
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

/** \file eapolParserMainInternal.h
 * \brief EAPOL frame parser internal definitions header file.
 *
 *
 * \see  eapolParseerMain.c,  eapolBuilderMain.h,  eapolBuilderMain.c, eapolParserMain.h
*/


/****************************************************************************
 *                                                                          *
 *   MODULE:                                                                *
 *   PURPOSE:                                                               *
 *   CREATOR: Alexander Sirotkin.                                           *
 *            Demiurg@ti.com                                                *
 *                                                                          *
 ****************************************************************************/

#ifndef  _KEY_TYPES_H
#define  _KEY_TYPES_H

#include "tidef.h"

/* Constatnts */


#define SESSION_KEY_LEN				16

/* Structures */

typedef struct
{
	TI_UINT32	keyId;                          /**< Decoded key Id */
	TI_UINT32	keyLen;							/**< Key length */
	char	*pData;							/**< Pointer to the material to derive the key from */
} encodedKeyMaterial_t;


#endif /*  _EAPOL_PARSER_INTERNAL_H*/
