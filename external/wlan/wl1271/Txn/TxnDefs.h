/*
 * TxnDefs.h
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


/** \file   TxnDefs.h 
 *  \brief  Common TXN definitions                                  
 *
 * These defintions are used also by the SDIO/SPI adapters, so they shouldn't
 *     base on any non-standart types (e.g. use unsigned int and not TI_UINT32)
 * 
 *  \see    
 */

#ifndef __TXN_DEFS_API_H__
#define __TXN_DEFS_API_H__


/************************************************************************
 * Defines
 ************************************************************************/
#define TXN_FUNC_ID_CTRL         0
#define TXN_FUNC_ID_BT           1
#define TXN_FUNC_ID_WLAN         2


/************************************************************************
 * Types
 ************************************************************************/
/* Transactions status (shouldn't override TI_OK and TI_NOK values) */
/** \enum ETxnStatus
 * \brief  Txn Transactions status
 *
 * \par Description
 * Txn Transactions status - shouldn't override TI_OK and TI_NOK values
 *
 * \sa
 */
typedef enum
{
    TXN_STATUS_NONE = 2,	/**< */
    TXN_STATUS_OK,         	/**< */
    TXN_STATUS_COMPLETE,  	/**< */ 
    TXN_STATUS_PENDING,    	/**< */
    TXN_STATUS_ERROR     	/**< */

} ETxnStatus;


#endif /*__TXN_DEFS_API_H__*/
