/*
 * p_buffer.h
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

#ifndef P_BUFFER
#define P_BUFFER


#define P_BUFFER_ADD_UINT8(_p_buffer, _uint8)                               \
        {                                                                   \
            *(TI_UINT8 *)(_p_buffer++) = _uint8;                            \
        }

#define P_BUFFER_ADD_UINT16(_p_buffer, _uint16)                             \
        {                                                                   \
            *(TI_UINT8 *)(_p_buffer++) = (TI_UINT8)(_uint16 & 0x00FF);                \
            *(TI_UINT8 *)(_p_buffer++) = (TI_UINT8)((_uint16 & 0xFF00) >> 8);         \
        }

#define P_BUFFER_ADD_UINT32(_p_buffer, _uint32)                             \
        {                                                                   \
            *(TI_UINT8 *)(_p_buffer++) = (TI_UINT8)(_uint32 & 0x000000FF);            \
            *(TI_UINT8 *)(_p_buffer++) = (TI_UINT8)((_uint32 & 0x0000FF00) >> 8);     \
            *(TI_UINT8 *)(_p_buffer++) = (TI_UINT8)((_uint32 & 0x00FF0000) >> 16);    \
            *(TI_UINT8 *)(_p_buffer++) = (TI_UINT8)((_uint32 & 0xFF000000) >> 24);    \
        }

#define P_BUFFER_ADD_DATA(_p_buffer, _p_data, _len)                         \
        {                                                                   \
            memcpy(_p_buffer, _p_data, _len);                               \
            _p_buffer += _len;                                              \
        }         

#define P_BUFFER_GET_UINT8(_p_buffer, _uint8)                               \
        {                                                                   \
            _uint8 = *(TI_UINT8 *)(_p_buffer++);                            \
        }

#define P_BUFFER_GET_UINT16(_p_buffer, _uint16)                             \
        {                                                                   \
            _uint16 = *(TI_UINT8 *)(_p_buffer++);                           \
            _uint16 |= (*(TI_UINT8 *)(_p_buffer++) << 8);                   \
        }


#define P_BUFFER_GET_UINT32(_p_buffer, _uint32)                             \
        {                                                                   \
            _uint32 = *(TI_UINT8 *)(_p_buffer++);                           \
            _uint32 |= (*(TI_UINT8 *)(_p_buffer++) << 8);                   \
            _uint32 |= (*(TI_UINT8 *)(_p_buffer++) << 16);                  \
            _uint32 |= (*(TI_UINT8 *)(_p_buffer++) << 24);                  \
        }

#define P_BUFFER_ADD_HDR_PARAMS(_p_buffer, _op, _status)                    \
        {                                                                   \
            *(TI_UINT8 *)(_p_buffer + 0) = (_op & 0x00FF);                  \
            *(TI_UINT8 *)(_p_buffer + 1) = ((_op & 0xFF00) >> 8);           \
            *(TI_UINT8 *)(_p_buffer + 2) = _status;                         \
            _p_buffer += 3;                                                 \
        }

#endif
