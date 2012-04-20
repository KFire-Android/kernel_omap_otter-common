/*
 * tidef.h
 *
 * Copyright(c) 1998 - 2010 Texas Instruments. All rights reserved.      
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

#ifndef TIDEF_H
#define TIDEF_H

#include "osTIType.h"

/** \file  tidef.h 
 * \brief TI User Definitions APIs 
 * \n\n
 * Note: TI_ prefix implies TI wrapping of primitives which is used for partability.
 * E.g. using these interfaces enables porting between different OS under these 
 * interfaces without user notice.
 * \n\n
 */

/**********************
 *	Definitions
 **********************/

/**
 * \def TI_FALSE
 * \brief False value
 */
#define TI_FALSE                    0
/**
 * \def TI_TRUE
 * \brief True value
 */
#define TI_TRUE                     1

/**
 * \def TI_OK
 * \brief OK return value
 */
#define TI_OK                       0
/**
 * \def TI_NOK
 * \brief NOT OK return value
 */
#define TI_NOK                      1
/**
 * \def MAC_ADDR_LEN
 * \brief Length of Standart MAC address
 */
#define MAC_ADDR_LEN                6
/**
 * \def IP_V4_ADDR_LEN
 * \brief Length of Standart IP V4 address
 */
#define REGISTER_SIZE		        4
#define IP_V4_ADDR_LEN              4
/**
 * \def IP_V4_ADDR_LEN
 * \brief Length of Standart IP V6 address
 */
#define IP_V6_ADDR_LEN              6

/**********************
 *	Macros
 **********************/


#ifndef TI_MAX
/**
 * \def TI_MAX
 * \brief Macro which returns the maximum of two values
 */
#define TI_MAX(a,b)                 (((a) > (b)) ? (a) : (b))
#endif

#ifndef TI_MIN
/**
 * \def TI_MAX
 * \brief Macro which returns the minimum of two values
 */
#define TI_MIN(a,b)                 (((a) < (b)) ? (a) : (b))
#endif

#ifndef NULL
/**
 * \def NULL
 * \brief Macro which returns NULL
 */
#define NULL                        ((void *)0)
#endif

/**
 * \def TI_VOIDCAST
 * \brief Macro which Casts to void
 */
#ifndef TI_VOIDCAST 
#define TI_VOIDCAST(p)             ((void)p)
#endif


#ifndef SIZE_ARR
/**
 * \def SIZE_ARR
 * \brief Macro which returns number of elements in array a
 */
#define SIZE_ARR(a)                 (sizeof(a)/sizeof(a[0]))
#endif


#ifndef TI_FIELD_OFFSET
/**
 * \def TI_FIELD_OFFSET
 * \brief Macro which returns a field offset from structure begine
 */
#define TI_FIELD_OFFSET(type,field)    ((TI_UINT32)(&(((type*)0)->field)))
#endif                                 


#ifndef TI_BIT
#define TI_BIT(x)                   (1 << (x))
#endif


#ifndef IS_MASK_ON
/**
 * \def IS_MASK_ON
 * \brief Macro which returns True if bitmask in field is on (==1) \n
 * Otherwise returns False
 */
#define IS_MASK_ON(  field, bitmask ) (  (bitmask) == ( (field) &  (bitmask) ) )
#endif

#ifndef IS_MASK_OFF
/**
 * \def IS_MASK_OFF
 * \brief Macro which returns True if bitmask in field is off (==0) \n
 * Otherwise returns False
 */
#define IS_MASK_OFF( field, bitmask ) ( ~(bitmask) == ( (field) | ~(bitmask) ) )
#endif


#ifndef INRANGE
/**
 * \def INRANGE
 * \brief Macro which returns True if value (x) in range (law <= x <= high) \n
 * Otherwise returns False
 */
#define INRANGE(x,low,high)         (((x) >= (low)) && ((x) <= (high)))
#endif

#ifndef OUTRANGE
/**
 * \def OUTRANGE
 * \brief Macro which returns True if value (x) out of range (x < law | x > high) \n
 * Otherwise returns False
 */
#define OUTRANGE(x,low,high)        (((x) < (low)) || ((x) > (high)))
#endif

/* Due to alignment exceptions MAC_COPY and MAC_EQUAL are done byte by byte */

/**
 * \def MAC_COPY
 * \brief Macro which copies 6 bytes source to 6 bytes destination \n
 * Due to alignment exceptions MAC_COPY is done byte by byte
 */
#define MAC_COPY(dst,src)           ((TI_UINT8*)(dst))[0] = ((TI_UINT8*)(src))[0]; \
                                    ((TI_UINT8*)(dst))[1] = ((TI_UINT8*)(src))[1]; \
                                    ((TI_UINT8*)(dst))[2] = ((TI_UINT8*)(src))[2]; \
                                    ((TI_UINT8*)(dst))[3] = ((TI_UINT8*)(src))[3]; \
                                    ((TI_UINT8*)(dst))[4] = ((TI_UINT8*)(src))[4]; \
                                    ((TI_UINT8*)(dst))[5] = ((TI_UINT8*)(src))[5]
/**
 * \def MAC_EQUAL
 * \brief Macro which compares 6 bytes ofmac1 to 6 bytes of mac2 and returns True if all are equall \n
 * Otherwise returns False \n
 * Due to alignment exceptions MAC_EQUAL is done byte by byte
 */
#define MAC_EQUAL(mac1,mac2)        (((TI_UINT8*)(mac1))[0] == ((TI_UINT8*)(mac2))[0] && \
                                     ((TI_UINT8*)(mac1))[1] == ((TI_UINT8*)(mac2))[1] && \
                                     ((TI_UINT8*)(mac1))[2] == ((TI_UINT8*)(mac2))[2] && \
                                     ((TI_UINT8*)(mac1))[3] == ((TI_UINT8*)(mac2))[3] && \
                                     ((TI_UINT8*)(mac1))[4] == ((TI_UINT8*)(mac2))[4] && \
                                     ((TI_UINT8*)(mac1))[5] == ((TI_UINT8*)(mac2))[5])
/**
 * \def MAC_BROADCAST
 * \brief Macro which returns True if MAC address is broadcast (equals "\xff\xff\xff\xff\xff\xff") \n
 * Otherwise returns False
 */
#define MAC_BROADCAST(mac)          MAC_EQUAL (mac, "\xff\xff\xff\xff\xff\xff")
/**
 * \def MAC_NULL
 * \brief Macro which returns True if MAC address is Null (equals "\x0\x0\x0\x0\x0\x0") \n
 * Otherwise returns False
 */
#define MAC_NULL(mac)               MAC_EQUAL (mac, "\x0\x0\x0\x0\x0\x0")
/**
 * \def MAC_MULTICAST
 * \brief Macro which returns True if MAC address is Multicast\n
 * Otherwise returns False
 */
#define MAC_MULTICAST(mac)          ((mac)[0] & 0x01)
/**
 * \def IP_COPY
 * \brief Macro which copies IP V4 source to IP V4 destination 
 */
#define IP_COPY(dst,src)            *((TI_UINT32*)(dst)) = *((TI_UINT32*)(src))
/**
 * \def BYTE_SWAP_WORD
 * \brief Macro which swaps Word's bytes. Used for Endian handling
 */
#define BYTE_SWAP_WORD(x)           ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))
/**
 * \def BYTE_SWAP_LONG
 * \brief Macro which swaps Long's bytes. Used for Endian handling
 */
#define BYTE_SWAP_LONG(x)           ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
                                     (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))


/* TI always supports Little Endian */
#if defined (__BYTE_ORDER_BIG_ENDIAN)

#define WLANTOHL(x)                 (x)
#define WLANTOHS(x)                 (x)
#define HTOWLANL(x)                 (x)
#define HTOWLANS(x)                 (x)

#define ENDIAN_HANDLE_WORD(x)       BYTE_SWAP_WORD (x)
#define ENDIAN_HANDLE_LONG(x)       BYTE_SWAP_LONG (x)

#define INT64_LOWER(x)              *(((TI_UINT32*)&(x))+1)
#define INT64_HIGHER(x)             *((TI_UINT32*)&(x))

#define COPY_WLAN_WORD(dst,src)     ((TI_UINT8 *)(dst))[0] = ((TI_UINT8 *)(src))[1]; \
                                    ((TI_UINT8 *)(dst))[1] = ((TI_UINT8 *)(src))[0]
#define COPY_WLAN_LONG(dst,src)     ((TI_UINT8 *)(dst))[0] = ((TI_UINT8 *)(src))[3]; \
                                    ((TI_UINT8 *)(dst))[1] = ((TI_UINT8 *)(src))[2]; \
                                    ((TI_UINT8 *)(dst))[2] = ((TI_UINT8 *)(src))[1]; \
                                    ((TI_UINT8 *)(dst))[3] = ((TI_UINT8 *)(src))[0]

#define SET_WLAN_WORD(dst,val)      ((TI_UINT8 *)(dst))[1] =  (val)        & 0xff; \
                                    ((TI_UINT8 *)(dst))[0] = ((val) >> 8)  & 0xff

#define SET_WLAN_LONG(dst,val)      ((TI_UINT8 *)(dst))[3] =  (val)        & 0xff; \
                                    ((TI_UINT8 *)(dst))[2] = ((val) >> 8)  & 0xff; \
                                    ((TI_UINT8 *)(dst))[1] = ((val) >> 16) & 0xff; \
                                    ((TI_UINT8 *)(dst))[0] = ((val) >> 24) & 0xff

#define WLAN_WORD(src)              (((TI_UINT8 *)(src))[1]) | (((TI_UINT8 *)(src))[0] << 8)
#define WLAN_LONG(src)              (((TI_UINT8 *)(src))[3]) | (((TI_UINT8 *)(src))[2] << 8) | (((TI_UINT8 *)(src))[1] << 16) | (((TI_UINT8 *)(src))[0] << 24)

#elif defined (__BYTE_ORDER_LITTLE_ENDIAN)

/**
 * \def WLANTOHL
 * \brief Macro which performs bytes swap of Long in Little Endian
 */
#define WLANTOHL(x)                 BYTE_SWAP_LONG (x)
/**
 * \def WLANTOHS
 * \brief Macro which performs bytes swap of Word in Little Endian
 */
#define WLANTOHS(x)                 BYTE_SWAP_WORD (x)
/**
 * \def HTOWLANL
 * \brief Macro which performs bytes swap of Long in Little Endian
 */
#define HTOWLANL(x)                 BYTE_SWAP_LONG (x)
/**
 * \def HTOWLANL
 * \brief Macro which performs bytes swap of Word in Little Endian
 */
#define HTOWLANS(x)                 BYTE_SWAP_WORD (x)

/**
 * \def ENDIAN_HANDLE_WORD
 * \brief Macro which handles Word in Little Endian 
 */
#define ENDIAN_HANDLE_WORD(x)       (x)
/**
 * \def ENDIAN_HANDLE_WORD
 * \brief Macro which handles Long in Little Endian 
 */
#define ENDIAN_HANDLE_LONG(x)       (x)

/**
 * \def INT64_HIGHER
 * \brief Macro which returns the content of higher address of INT64 variable in Little Endian
 */
#define INT64_HIGHER(x)             *(((TI_UINT32*)&(x))+1)
/**
 * \def INT64_LOWER
 * \brief Macro which returns the content of lower address of INT64 variable in Little Endian
 */
#define INT64_LOWER(x)              *((TI_UINT32*)&(x))

/**
 * \def COPY_WLAN_WORD
 * \brief Macro which copies word source to word destination byte by byte in Little Endian
 */
#define COPY_WLAN_WORD(dst,src)     ((TI_UINT8 *)(dst))[0] = ((TI_UINT8 *)(src))[0]; \
                                    ((TI_UINT8 *)(dst))[1] = ((TI_UINT8 *)(src))[1]
/**
 * \def COPY_WLAN_LONG
 * \brief Macro which copies long source to long destination byte by byte in Little Endian
 */
#define COPY_WLAN_LONG(dst,src)     ((TI_UINT8 *)(dst))[0] = ((TI_UINT8 *)(src))[0]; \
                                    ((TI_UINT8 *)(dst))[1] = ((TI_UINT8 *)(src))[1]; \
                                    ((TI_UINT8 *)(dst))[2] = ((TI_UINT8 *)(src))[2]; \
                                    ((TI_UINT8 *)(dst))[3] = ((TI_UINT8 *)(src))[3]
/**
 * \def SET_WLAN_WORD
 * \brief Macro which copies Word from val source to desrination in Little Endian
 */
#define SET_WLAN_WORD(dst,val)      ((TI_UINT8 *)(dst))[0] =  (val)        & 0xff; \
                                    ((TI_UINT8 *)(dst))[1] = ((val) >> 8)  & 0xff
/**
 * \def SET_WLAN_LONG
 * \brief Macro which copies Long from val source to desrination in Little Endian
 */
#define SET_WLAN_LONG(dst,val)      ((TI_UINT8 *)(dst))[0] =  (val)        & 0xff; \
                                    ((TI_UINT8 *)(dst))[1] = ((val) >> 8)  & 0xff; \
                                    ((TI_UINT8 *)(dst))[2] = ((val) >> 16) & 0xff; \
                                    ((TI_UINT8 *)(dst))[3] = ((val) >> 24) & 0xff
/**
 * \def WLAN_WORD
 * \brief Macro which returns Word value from source address in Little Endian
 */
#define WLAN_WORD(src)              (((TI_UINT8 *)(src))[0]) | (((TI_UINT8 *)(src))[1] << 8)
/**
 * \def WLAN_LONG
 * \brief Macro which returns Long value from source address in Little Endian
 */
#define WLAN_LONG(src)              (((TI_UINT8 *)(src))[0]) | (((TI_UINT8 *)(src))[1] << 8) | (((TI_UINT8 *)(src))[2] << 16) | (((TI_UINT8 *)(src))[3] << 24)
#else

#error "Must define byte order (BIG/LITTLE ENDIAN)"

#endif


/**********************
 *	types
 **********************/

/**
 * \typedef TI_HANDLE
 * \brief Handle type - Pointer to void
 */
typedef void*                       TI_HANDLE;
/**
 * \typedef TI_BOOL
 * \brief Boolean type
 * \n
 * Used for indicating True or False ( TI_TRUE | TI_FALSE )
 */
typedef TI_UINT32                   TI_BOOL;
/**
 * \typedef TI_STATUS
 * \brief Return Status type
 * \n
 * Used as return status ( TI_OK | TI_NOK | TI_PENDING )
 */
typedef TI_UINT32                   TI_STATUS;
/**
 * \typedef TMacAddr
 * \brief MAC Address Type
 * \n
 * A buffer (size of Standart MAC Address Length in bytes) which holds a MAC address
 */
typedef TI_UINT8                    TMacAddr [MAC_ADDR_LEN];
/**
 * \typedef TIpAddr
 * \brief IP V4 Address Type
 * \n
 * A buffer (size of Standart IP V4 Address Length in bytes) which holds IP V4 address
 */
typedef TI_UINT8                    TIpAddr [IP_V4_ADDR_LEN];

#endif /* TIDEF_H */


