/*
 * rate.h
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


/** \file  rate.h
 *  \brief Rate conversion
 *
 *  \see   rate.c
 */

#ifndef RATE_H
#define RATE_H

#include "TWDriver.h"


typedef enum
{
    NET_BASIC_MASK      = 0x80,
    NET_RATE_AUTO       = 0x00,
    NET_RATE_1M         = 0x02,
    NET_RATE_2M         = 0x04,
    NET_RATE_5_5M       = 0x0B,
    NET_RATE_11M        = 0x16,
    NET_RATE_22M        = 0x2C,
    NET_RATE_6M         = 0x0C,
    NET_RATE_9M         = 0x12,
    NET_RATE_12M        = 0x18,
    NET_RATE_18M        = 0x24,
    NET_RATE_24M        = 0x30,
    NET_RATE_36M        = 0x48,
    NET_RATE_48M        = 0x60,
    NET_RATE_54M        = 0x6C,
    NET_RATE_MCS0       = 0x0D, /* MCS0 6.5M */
    NET_RATE_MCS1       = 0x1A, /* MCS1 13M */
    NET_RATE_MCS2       = 0x27, /* MCS2 19.5M */
    NET_RATE_MCS3       = 0x34, /* MCS3 26M */
    NET_RATE_MCS4       = 0x4E, /* MCS4 39M */
    NET_RATE_MCS5       = 0x68, /* MCS5 52M */
    NET_RATE_MCS6       = 0x75, /* MCS6 58.5M */
    NET_RATE_MCS7       = 0x7F  /* MCS7 65M */

} ENetRate;


typedef enum 
{
    NET_RATE_1M_BASIC   = (NET_RATE_1M   | NET_BASIC_MASK),
    NET_RATE_2M_BASIC   = (NET_RATE_2M   | NET_BASIC_MASK),
    NET_RATE_5_5M_BASIC = (NET_RATE_5_5M | NET_BASIC_MASK),
    NET_RATE_11M_BASIC  = (NET_RATE_11M  | NET_BASIC_MASK),
    NET_RATE_22M_BASIC  = (NET_RATE_22M  | NET_BASIC_MASK),
    NET_RATE_6M_BASIC   = (NET_RATE_6M   | NET_BASIC_MASK),
    NET_RATE_9M_BASIC   = (NET_RATE_9M   | NET_BASIC_MASK),
    NET_RATE_12M_BASIC  = (NET_RATE_12M  | NET_BASIC_MASK),
    NET_RATE_18M_BASIC  = (NET_RATE_18M  | NET_BASIC_MASK),
    NET_RATE_24M_BASIC  = (NET_RATE_24M  | NET_BASIC_MASK),
    NET_RATE_36M_BASIC  = (NET_RATE_36M  | NET_BASIC_MASK),
    NET_RATE_48M_BASIC  = (NET_RATE_48M  | NET_BASIC_MASK),
    NET_RATE_54M_BASIC  = (NET_RATE_54M  | NET_BASIC_MASK),
    NET_RATE_MCS0_BASIC  = (NET_RATE_MCS0  | NET_BASIC_MASK),
    NET_RATE_MCS1_BASIC  = (NET_RATE_MCS1  | NET_BASIC_MASK),
    NET_RATE_MCS2_BASIC  = (NET_RATE_MCS2  | NET_BASIC_MASK),
    NET_RATE_MCS3_BASIC  = (NET_RATE_MCS3  | NET_BASIC_MASK),
    NET_RATE_MCS4_BASIC  = (NET_RATE_MCS4  | NET_BASIC_MASK),
    NET_RATE_MCS5_BASIC  = (NET_RATE_MCS5  | NET_BASIC_MASK),
    NET_RATE_MCS6_BASIC  = (NET_RATE_MCS6  | NET_BASIC_MASK),
    NET_RATE_MCS7_BASIC  = (NET_RATE_MCS7  | NET_BASIC_MASK)

} ENetRateBasic;


typedef enum
{
    BASIC_RATE_SET_1_2                  = 0,
    BASIC_RATE_SET_1_2_5_5_11           = 1,
    BASIC_RATE_SET_UP_TO_12             = 2,
    BASIC_RATE_SET_UP_TO_18             = 3,
    BASIC_RATE_SET_1_2_5_5_6_11_12_24   = 4,
    BASIC_RATE_SET_UP_TO_36             = 5,
    BASIC_RATE_SET_UP_TO_48             = 6,
    BASIC_RATE_SET_UP_TO_54             = 7,
    BASIC_RATE_SET_UP_TO_24             = 8,
    BASIC_RATE_SET_6_12_24              = 9,
    BASIC_RATE_SET_ALL_MCS_RATES        = 10
} EBasicRateSet;


/* Keep increasing define values - related to increasing suported rates */
typedef enum
{
    SUPPORTED_RATE_SET_1_2              = 0,
    SUPPORTED_RATE_SET_1_2_5_5_11       = 1,
    SUPPORTED_RATE_SET_1_2_5_5_11_22    = 2,
    SUPPORTED_RATE_SET_UP_TO_18         = 3,
    SUPPORTED_RATE_SET_UP_TO_24         = 4,
    SUPPORTED_RATE_SET_UP_TO_36         = 5,
    SUPPORTED_RATE_SET_UP_TO_48         = 6,
    SUPPORTED_RATE_SET_UP_TO_54         = 7,
    SUPPORTED_RATE_SET_ALL              = 8,
    SUPPORTED_RATE_SET_ALL_OFDM         = 9,
    SUPPORTED_RATE_SET_ALL_MCS_RATES    = 10

} ESupportedRateSet;


typedef enum
{
    DRV_MODULATION_NONE     = 0,
    DRV_MODULATION_CCK      = 1,
    DRV_MODULATION_PBCC     = 2,
    DRV_MODULATION_QPSK     = 3,
    DRV_MODULATION_OFDM     = 4

} EModulationType;


#define NET_BASIC_RATE(rate)       ((rate) & NET_BASIC_MASK)
#define NET_ACTIVE_RATE(rate)      (!NET_BASIC_RATE (rate))


ERate     rate_NumberToDrv (TI_UINT32 rate);
TI_UINT32 rate_DrvToNumber (ERate eRate);
ERate     rate_NetToDrv (TI_UINT32 rate);
ENetRate  rate_DrvToNet (ERate eRate);
TI_STATUS rate_DrvBitmapToNetStr (TI_UINT32 uSuppRatesBitMap, TI_UINT32 uBasicRatesBitMap, TI_UINT8 *string, TI_UINT32 *len, TI_UINT32 *pFirstOfdmRate);
TI_STATUS rate_DrvBitmapToNetStrIncluding11n (TI_UINT32 uSuppRatesBitMap, TI_UINT32 uBasicRatesBitMap, TI_UINT8 *string, TI_UINT32 *pFirstOfdmRate);
TI_STATUS rate_NetStrToDrvBitmap (TI_UINT32 *pBitMap, TI_UINT8 *string, TI_UINT32 len);
TI_STATUS rate_NetBasicStrToDrvBitmap (TI_UINT32 *pBitMap, TI_UINT8 *string, TI_UINT32 len);
TI_STATUS rate_McsNetStrToDrvBitmap (TI_UINT32 *pBitMap, TI_UINT8 *string);
TI_STATUS rate_DrvBitmapToHwBitmap (TI_UINT32 uDrvBitmap, TI_UINT32 *pHwBitmap);
TI_STATUS rate_PolicyToDrv (ETxRateClassId ePolicyRate, ERate *eAppRate);
TI_UINT32 rate_BasicToDrvBitmap (EBasicRateSet eBasicRateSet, TI_BOOL bDot11a);
TI_UINT32 rate_SupportedToDrvBitmap (ESupportedRateSet eSupportedRateSet, TI_BOOL bDot11a);

ERate     rate_GetMaxFromDrvBitmap (TI_UINT32 uBitMap);
ENetRate  rate_GetMaxBasicFromStr (TI_UINT8 *pRatesString, TI_UINT32 len, ENetRate eMaxRate);
ENetRate  rate_GetMaxActiveFromStr (TI_UINT8 *pRatesString, TI_UINT32 len, ENetRate eMaxRate);

TI_STATUS rate_ValidateVsBand (TI_UINT32 *pSupportedMask, TI_UINT32 *pBasicMask, TI_BOOL bDot11a);

TI_UINT32 rate_GetDrvBitmapForDefaultBasicSet (void);
TI_UINT32 rate_GetDrvBitmapForDefaultSupporteSet (void);

#endif


