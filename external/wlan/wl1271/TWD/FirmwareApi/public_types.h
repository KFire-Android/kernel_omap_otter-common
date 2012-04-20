/*
 * public_types.h
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

/**********************************************************************************************************************

  FILENAME:       public_types.h

  DESCRIPTION:    Basic types and general macros, bit manipulations, etc.



***********************************************************************************************************************/
#ifndef PUBLIC_TYPES_H
#define PUBLIC_TYPES_H
 

/******************************************************************************

    Basic definitions

******************************************************************************/
#ifndef uint8
typedef unsigned char   uint8;
#endif
#ifndef uint16
typedef unsigned short  uint16;
#endif
#ifndef uint32
typedef unsigned long int    uint32;
#endif
#ifndef uint64
typedef unsigned long long    uint64;
#endif


#ifndef int8
typedef signed char     int8;
#endif
#ifndef int16
typedef short           int16;
#endif
#ifndef int32
typedef long int        int32;
#endif
#ifndef int64
typedef long long       int64;
#endif


#ifdef HOST_COMPILE
    #ifndef TI_TRUE
    #define TI_TRUE  1
    #endif
    #ifndef TI_FALSE
    #define TI_FALSE 0
    #endif
#else
    #ifndef TRUE
    #define TRUE  1
    #endif
    #ifndef FALSE
    #define FALSE 0
    #endif
    #define STATIC			static
    #define INLINE			inline
#endif

/* !! LAC - NULL definition conflicts with the compilers version.
   I redid this definition to the ANSI version....
    #define NULL 0
*/
#if !defined( NULL )
#if defined( __cplusplus )
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

/* Bool_e should be used when we need it to be a byte. */
typedef uint8           Bool_e;

/* Bool32 should be used whenever possible for efficiency */
typedef uint32          Bool32;

/* to align enum to 32/16 bits */
#define MAX_POSITIVE32 0x7FFFFFFF
#define MAX_POSITIVE16 0x7FFF
#define MAX_POSITIVE8  0x7F

#define MAC_ADDR_SIZE							6   /* In Bytes */
#define MAC_ADDRESS_MANUFACTURE_TYPE_LENGHT		3   /* In Bytes */
#define MAC_ADDRESS_STATION_ID_LENGHT			3   /* In Bytes */

#ifdef HOST_COMPILE
#else
typedef struct macAddress_t
{
    uint8 addr[MAC_ADDR_SIZE];
}macAddress_t;
#endif


#define  BIT_0    0x00000001
#define  BIT_1    0x00000002
#define  BIT_2    0x00000004
#define  BIT_3    0x00000008
#define  BIT_4    0x00000010
#define  BIT_5    0x00000020
#define  BIT_6    0x00000040
#define  BIT_7    0x00000080
#define  BIT_8    0x00000100
#define  BIT_9    0x00000200
#define  BIT_10   0x00000400
#define  BIT_11   0x00000800
#define  BIT_12   0x00001000
#define  BIT_13   0x00002000
#define  BIT_14   0x00004000
#define  BIT_15   0x00008000
#define  BIT_16   0x00010000
#define  BIT_17   0x00020000
#define  BIT_18   0x00040000
#define  BIT_19   0x00080000
#define  BIT_20   0x00100000
#define  BIT_21   0x00200000
#define  BIT_22   0x00400000
#define  BIT_23   0x00800000
#define  BIT_24   0x01000000
#define  BIT_25   0x02000000
#define  BIT_26   0x04000000
#define  BIT_27   0x08000000
#define  BIT_28   0x10000000
#define  BIT_29   0x20000000
#define  BIT_30   0x40000000
#define  BIT_31   0x80000000

#define  BIT_32   0x00000001
#define  BIT_33   0x00000002
#define  BIT_34   0x00000004
#define  BIT_35   0x00000008
#define  BIT_36   0x00000010
#define  BIT_37   0x00000020
#define  BIT_38   0x00000040
#define  BIT_39   0x00000080
#define  BIT_40   0x00000100
#define  BIT_41   0x00000200
#define  BIT_42   0x00000400
#define  BIT_43   0x00000800
#define  BIT_44   0x00001000
#define  BIT_45   0x00002000
#define  BIT_46   0x00004000
#define  BIT_47   0x00008000
#define  BIT_48   0x00010000
#define  BIT_49   0x00020000
#define  BIT_50   0x00040000
#define  BIT_51   0x00080000
#define  BIT_52   0x00100000
#define  BIT_53   0x00200000
#define  BIT_54   0x00400000
#define  BIT_55   0x00800000
#define  BIT_56   0x01000000
#define  BIT_57   0x02000000
#define  BIT_58   0x04000000
#define  BIT_59   0x08000000
#define  BIT_60   0x10000000
#define  BIT_61   0x20000000
#define  BIT_62   0x40000000
#define  BIT_63   0x80000000


/******************************************************************************

    CHANNELS, BAND & REG DOMAINS definitions

******************************************************************************/


typedef uint8 Channel_e;

typedef enum
{
    RADIO_BAND_2_4GHZ = 0,  /* 2.4 Ghz band */
    RADIO_BAND_5GHZ = 1,    /* 5 Ghz band */
    RADIO_BAND_JAPAN_4_9_GHZ = 2,
    DEFAULT_BAND = RADIO_BAND_2_4GHZ,
    INVALID_BAND = 0x7E,
    MAX_RADIO_BANDS = 0x7F
} RadioBand_enum;

#ifdef HOST_COMPILE
typedef uint8 RadioBand_e;
#else
typedef RadioBand_enum RadioBand_e;
#endif

/* The following enum is used in the FW for HIF interface only !!!!! */
typedef enum
{
    HW_BIT_RATE_1MBPS   = BIT_0 ,
    HW_BIT_RATE_2MBPS   = BIT_1 ,
    HW_BIT_RATE_5_5MBPS = BIT_2 ,
    HW_BIT_RATE_6MBPS   = BIT_3 ,
    HW_BIT_RATE_9MBPS   = BIT_4 ,
    HW_BIT_RATE_11MBPS  = BIT_5 ,
    HW_BIT_RATE_12MBPS  = BIT_6 ,
    HW_BIT_RATE_18MBPS  = BIT_7 ,
    HW_BIT_RATE_22MBPS  = BIT_8 ,
    HW_BIT_RATE_24MBPS  = BIT_9 ,
    HW_BIT_RATE_36MBPS  = BIT_10,
    HW_BIT_RATE_48MBPS  = BIT_11,
    HW_BIT_RATE_54MBPS  = BIT_12,
    HW_BIT_RATE_MCS_0  	= BIT_13,
    HW_BIT_RATE_MCS_1  	= BIT_14,
    HW_BIT_RATE_MCS_2  	= BIT_15,
    HW_BIT_RATE_MCS_3  	= BIT_16,
    HW_BIT_RATE_MCS_4  	= BIT_17,
    HW_BIT_RATE_MCS_5  	= BIT_18,
    HW_BIT_RATE_MCS_6  	= BIT_19,
    HW_BIT_RATE_MCS_7  	= BIT_20
} EHwBitRate;

/* The following enum is used in the FW for HIF interface only !!!!! */
typedef enum
{
    txPolicyMcs7 = 0,
    txPolicyMcs6,
    txPolicyMcs5,
    txPolicyMcs4,
    txPolicyMcs3,
    txPolicyMcs2,
    txPolicyMcs1,
    txPolicyMcs0,
    txPolicy54,
    txPolicy48,
    txPolicy36,
    txPolicy24,
    txPolicy22,
    txPolicy18,
    txPolicy12,
    txPolicy11,
    txPolicy9,
    txPolicy6,
    txPolicy5_5,
    txPolicy2,
    txPolicy1,
    MAX_NUM_OF_TX_RATES_IN_CLASS,
    TX_RATE_INDEX_ENUM_MAX_SIZE = 0xFF
} ETxRateClassId;




#define SHORT_PREAMBLE_BIT   BIT_0 /* CCK or Barker depending on the rate */
#define OFDM_RATE_BIT        BIT_6
#define PBCC_RATE_BIT        BIT_7


typedef enum
{
    CCK_LONG = 0,
    CCK_SHORT = SHORT_PREAMBLE_BIT,
    PBCC_LONG = PBCC_RATE_BIT,
    PBCC_SHORT = PBCC_RATE_BIT | SHORT_PREAMBLE_BIT,
    OFDM = OFDM_RATE_BIT
} Mod_enum;

#ifdef HOST_COMPILE
typedef  uint8 Mod_e;
#else
typedef  Mod_enum Mod_e;
#endif


typedef uint16 BasicRateSet_t;


/******************************************************************************

Transmit-Descriptor RATE-SET field definitions...

******************************************************************************/

typedef uint32 EHwRateBitFiled;/* set with EHwBitRate values */

#ifdef HOST_COMPILE
typedef uint8  TxRateIndex_t;  /* set with ETxRateClassId values */
#else 
typedef ETxRateClassId TxRateIndex_t;
#endif

/******************************************************************************
 
    CHIP_ID definitions
 
******************************************************************************/
#define TNETW1150_PG10_CHIP_ID          0x04010101
#define TNETW1150_PG11_CHIP_ID          0x04020101
#define TNETW1150_CHIP_ID               0x04030101  /* 1150 PG2.0, 1250, 1350, 1450*/
#define TNETW1350A_CHIP_ID              0x06010101
#define TNETW1251_CHIP_ID_PG1_0         0x07010101
#define TNETW1251_CHIP_ID_PG1_1         0x07020101
#define TNETW1251_CHIP_ID_PG1_2	        0x07030101
#define TNETW1273_CHIP_ID_PG1_0	        0x04030101
#define TNETW1273_CHIP_ID_PG1_1	        0x04030111 

#define CHECK_CHIP_ID(chipId) (CHIP_ID_B == chipId)

/******************************************************************************
Enable bits for SOC1251 PG1.2
******************************************************************************/
#define PDET_BINARY_OFFSET_EN   BIT_0
#define STOP_TOGGLE_MONADC_EN   BIT_1
#define RX_ADC_BIAS_DEC_EN      BIT_2
#define RX_LNB_AND_DIGI_GAIN_EN BIT_3


#endif /* PUBLIC_TYPES_H*/
