/*
 * Ethernet.h
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


/***************************************************************************/
/*                                                                         */
/*    MODULE:   Ethernet.h                                                     */
/*    PURPOSE:                                                             */
/*                                                                         */
/***************************************************************************/
#ifndef _ETHERNET_H_
#define _ETHERNET_H_


typedef struct
{
    TMacAddr        dst;
    TMacAddr        src;
    TI_UINT16       type;

} TEthernetHeader;
 

typedef struct
{
    TMacAddr        dst;
    TMacAddr        src;
    TI_UINT16       length;
    TI_UINT8        dsap;
    TI_UINT8        ssap;
    TI_UINT8        control;
    TI_UINT8        oui[3];
    TI_UINT16       type;

} TLlcSnapHeader;


#define ETHERTYPE_802_1D                        0x8100
#define ETHERTYPE_EAPOL                         0x888e
#define ETHERTYPE_PREAUTH_EAPOL                 0x88c7
#define ETHERTYPE_IP                            0x0800
#define ETHERTYPE_APPLE_AARP                    0x80f3
#define ETHERTYPE_DIX_II_IPX                    0x8137

#define ETHERNET_HDR_LEN                        14
#define IEEE802_3_HDR_LEN                       14 
#define LLC_SNAP_HDR_LEN                        20

#define SNAP_CHANNEL_ID                         0xAA
#define LLC_CONTROL_UNNUMBERED_INFORMATION      0x03
#define ETHERNET_MAX_PAYLOAD_SIZE               1500

#define SNAP_OUI_802_1H_BYTE0                   0x00
#define SNAP_OUI_802_1H_BYTE1                   0x00
#define SNAP_OUI_802_1H_BYTE2                   0xf8
#define SNAP_OUI_802_1H_BYTES  { SNAP_OUI_802_1H_BYTE0, SNAP_OUI_802_1H_BYTE1, SNAP_OUI_802_1H_BYTE2 }

#define SNAP_OUI_RFC1042_BYTE0                  0x00
#define SNAP_OUI_RFC1042_BYTE1                  0x00
#define SNAP_OUI_RFC1042_BYTE2                  0x00
#define SNAP_OUI_RFC1042_LEN                    3
#define SNAP_OUI_RFC1042_BYTES { SNAP_OUI_RFC1042_BYTE0, SNAP_OUI_RFC1042_BYTE1, SNAP_OUI_RFC1042_BYTE2 }


#endif
