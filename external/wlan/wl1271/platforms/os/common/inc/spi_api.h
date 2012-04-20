/*
 * spi_api.h
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


/*
 * inc/spi_api.h
 *
 * Header for SPI API test interface
 *
 */

#ifndef SPI_API_H
#define SPI_API_H


typedef void (*request_callback_t) (void *data, int status);


typedef enum 
{
    SPI_OK                =  0,
    SPI_TXN_COMPLETE      =  0,
    SPI_TXN_PENDING       =  1,
    SPI_ERR_UNKNOWN       = -1,
    SPI_ERR_BUS_BUSY      = -2,
    SPI_ERR_QUEUE_FULL    = -3,
    SPI_ERR_ALLOC_MEM     = -4,
    SPI_ERR_ASYNC_TIMEOUT = -5, 
    SPI_ERR_WRONG_LENGTH  = -6,

} SPI_Status_e; 


void* SPI_Open         (void* hOS);
int   SPI_Close        (void* hSPI);
int   SPI_Write        (void* hSPI, TI_UINT8 *data, TI_UINT32 length, request_callback_t fCb, void *pCb, int more);                     
int   SPI_Read         (void* hSPI, TI_UINT8 *data, TI_UINT32 length, request_callback_t fCb, void *pCb, int more);                      
int   SPI_WriteRead    (void* hSPI, TI_UINT8 *data, TI_UINT32 length, TI_UINT8 *rdata, TI_UINT32 rlength, request_callback_t fCb, void* pCb, int more);
int   SPI_WriteSync    (void* hSPI, TI_UINT8 *data, TI_UINT32 length, int more);
int   SPI_ReadSync     (void* hSPI, TI_UINT8 *data, TI_UINT32 length);


#endif /* SPI_API_H */

