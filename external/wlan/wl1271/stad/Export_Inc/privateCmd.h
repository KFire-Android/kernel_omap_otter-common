/*
 * privateCmd.h
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


/****************************************************************************/
/*																			*/
/*    MODULE:   privateCmd.h													*/
/*    PURPOSE:																*/
/*																			*/
/****************************************************************************/
#ifndef _PVT_CMD_H_
#define _PVT_CMD_H_


/** \file  privateCmd.h 
 * \brief Private Command	\n
 * This file contains all the data structures and definitions which are needed for Private Command
 * \n\n
 *
 * \n\n
 */

#include "osTIType.h"

/***********/
/* defines */
/***********/

/** \def PRIVATE_CMD_SET_FLAG
 * \brief Bitmaks of bit which indicates that the Command is SET Command
 */
#define PRIVATE_CMD_SET_FLAG	0x00000001
/** \def PRIVATE_CMD_GET_FLAG
 * \brief Bitmaks of bit which indicates that the Command is GET Command
 */
#define PRIVATE_CMD_GET_FLAG	0x00000002


/*********/
/* types */
/*********/

/** \struct ti_private_cmd_t
 * \brief TI Private Command
 * 
 * \par Description
 * This Struct defines the Parameters needed for performing TI Private Command 
 * 
 * \sa
 */
typedef struct
{
	TI_UINT32 	cmd;   			/**< Number of command to execute - configMgr parameter name			*/
	TI_UINT32 	flags; 			/**< Command action type (PRIVATE_CMD_SET_FLAG | PRIVATE_CMD_GET_FLAG) 	*/
	void* 		in_buffer; 		/**< Pointer to Input Buffer											*/
	TI_UINT32	in_buffer_len; 	/**< Input buffer length 												*/
	void* 		out_buffer;		/**< Pointer to Output buffer											*/
	TI_UINT32	out_buffer_len;	/**< Output buffer length 												*/
} ti_private_cmd_t; 


/*************/
/* functions */
/*************/

#endif  /* _PVT_CMD_H_*/
        
