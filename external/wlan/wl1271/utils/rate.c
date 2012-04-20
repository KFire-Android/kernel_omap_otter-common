/*
 * rate.c
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


/** \file  rate.c
 *  \brief Rate conversion
 *
 *  \see   rate.h
 */
#define __FILE_ID__  FILE_ID_131
#include "tidef.h"
#include "rate.h"

ERate rate_NetToDrv (TI_UINT32 rate)
{
    switch (rate)
    {
        case NET_RATE_1M:
        case NET_RATE_1M_BASIC:
            return DRV_RATE_1M;

        case NET_RATE_2M:
        case NET_RATE_2M_BASIC:
            return DRV_RATE_2M;

        case NET_RATE_5_5M:
        case NET_RATE_5_5M_BASIC:
            return DRV_RATE_5_5M;

        case NET_RATE_11M:
        case NET_RATE_11M_BASIC:
            return DRV_RATE_11M;

        case NET_RATE_22M:
        case NET_RATE_22M_BASIC:
            return DRV_RATE_22M;

        case NET_RATE_6M:
        case NET_RATE_6M_BASIC:
            return DRV_RATE_6M;

        case NET_RATE_9M:
        case NET_RATE_9M_BASIC:
            return DRV_RATE_9M;

        case NET_RATE_12M:
        case NET_RATE_12M_BASIC:
            return DRV_RATE_12M;

        case NET_RATE_18M:
        case NET_RATE_18M_BASIC:
            return DRV_RATE_18M;

        case NET_RATE_24M:
        case NET_RATE_24M_BASIC:
            return DRV_RATE_24M;

        case NET_RATE_36M:
        case NET_RATE_36M_BASIC:
            return DRV_RATE_36M;

        case NET_RATE_48M:
        case NET_RATE_48M_BASIC:
            return DRV_RATE_48M;

        case NET_RATE_54M:
        case NET_RATE_54M_BASIC:
            return DRV_RATE_54M;

        case NET_RATE_MCS0:
        case NET_RATE_MCS0_BASIC:
            return DRV_RATE_MCS_0;

        case NET_RATE_MCS1:
        case NET_RATE_MCS1_BASIC:
            return DRV_RATE_MCS_1;

        case NET_RATE_MCS2:
        case NET_RATE_MCS2_BASIC:
            return DRV_RATE_MCS_2;

        case NET_RATE_MCS3:
        case NET_RATE_MCS3_BASIC:
            return DRV_RATE_MCS_3;

        case NET_RATE_MCS4:
        case NET_RATE_MCS4_BASIC:
            return DRV_RATE_MCS_4;

        case NET_RATE_MCS5:
        case NET_RATE_MCS5_BASIC:
            return DRV_RATE_MCS_5;

        case NET_RATE_MCS6:
        case NET_RATE_MCS6_BASIC:
            return DRV_RATE_MCS_6;

        case NET_RATE_MCS7:
        case NET_RATE_MCS7_BASIC:
            return DRV_RATE_MCS_7;

        default:
            return DRV_RATE_INVALID;
    }
}

/************************************************************************
 *                        hostToNetworkRate                         *
 ************************************************************************
DESCRIPTION: Translates a host rate (1, 2, 3, ....) to network rate (0x02, 0x82, 0x84, etc...) 
                                                                                                   
INPUT:      rate        -   Host rate

OUTPUT:     


RETURN:     Network rate if the input rate is valid, otherwise returns 0.

************************************************************************/
ENetRate rate_DrvToNet (ERate rate)
{
    switch (rate)
    {
        case DRV_RATE_AUTO:
            return NET_RATE_AUTO;

        case DRV_RATE_1M:
            return NET_RATE_1M;

        case DRV_RATE_2M:
            return NET_RATE_2M;

        case DRV_RATE_5_5M:
            return NET_RATE_5_5M;

        case DRV_RATE_11M:
            return NET_RATE_11M;

        case DRV_RATE_22M:
            return NET_RATE_22M;

        case DRV_RATE_6M:
            return NET_RATE_6M;

        case DRV_RATE_9M:
            return NET_RATE_9M;

        case DRV_RATE_12M:
            return NET_RATE_12M;

        case DRV_RATE_18M:
            return NET_RATE_18M;

        case DRV_RATE_24M:
            return NET_RATE_24M;

        case DRV_RATE_36M:
            return NET_RATE_36M;

        case DRV_RATE_48M:
            return NET_RATE_48M;

        case DRV_RATE_54M:
            return NET_RATE_54M;

        case DRV_RATE_MCS_0:
            return NET_RATE_MCS0;

        case DRV_RATE_MCS_1:
            return NET_RATE_MCS1;
    
        case DRV_RATE_MCS_2:
            return NET_RATE_MCS2;
    
        case DRV_RATE_MCS_3:
            return NET_RATE_MCS3;
    
        case DRV_RATE_MCS_4:
            return NET_RATE_MCS4;
    
        case DRV_RATE_MCS_5:
            return NET_RATE_MCS5;
    
        case DRV_RATE_MCS_6:
            return NET_RATE_MCS6;
    
        case DRV_RATE_MCS_7:
            return NET_RATE_MCS7;

        default:
            return NET_RATE_AUTO;
    }
}

/***************************************************************************
*                   getMaxActiveRatefromBitmap                             *
****************************************************************************
* DESCRIPTION:  
*
* INPUTS:       hCtrlData - the object
*               
* OUTPUT:       
*
* RETURNS:      
***************************************************************************/
ERate rate_GetMaxFromDrvBitmap (TI_UINT32 uRateBitMap)
{
    if (uRateBitMap & DRV_RATE_MASK_MCS_7_OFDM)
    {
        return DRV_RATE_MCS_7;
    }

    if (uRateBitMap & DRV_RATE_MASK_MCS_6_OFDM)
    {
        return DRV_RATE_MCS_6;
    }

    if (uRateBitMap & DRV_RATE_MASK_MCS_5_OFDM)
    {
        return DRV_RATE_MCS_5;
    }

    if (uRateBitMap & DRV_RATE_MASK_MCS_4_OFDM)
    {
        return DRV_RATE_MCS_4;
    }

    if (uRateBitMap & DRV_RATE_MASK_MCS_3_OFDM)
    {
        return DRV_RATE_MCS_3;
    }

    if (uRateBitMap & DRV_RATE_MASK_MCS_2_OFDM)
    {
        return DRV_RATE_MCS_2;
    }

    if (uRateBitMap & DRV_RATE_MASK_MCS_1_OFDM)
    {
        return DRV_RATE_MCS_1;
    }

    if (uRateBitMap & DRV_RATE_MASK_MCS_0_OFDM)
    {
        return DRV_RATE_MCS_0;
    }

    if (uRateBitMap & DRV_RATE_MASK_54_OFDM)
    {
        return DRV_RATE_54M;
    }

    if (uRateBitMap & DRV_RATE_MASK_48_OFDM)
    {
        return DRV_RATE_48M;
    }

    if (uRateBitMap & DRV_RATE_MASK_36_OFDM)
    {
        return DRV_RATE_36M;
    }

    if (uRateBitMap & DRV_RATE_MASK_24_OFDM)
    {
        return DRV_RATE_24M;
    }

    if (uRateBitMap & DRV_RATE_MASK_22_PBCC)
    {
        return DRV_RATE_22M;
    }

    if (uRateBitMap & DRV_RATE_MASK_18_OFDM)
    {
        return DRV_RATE_18M;
    }

    if (uRateBitMap & DRV_RATE_MASK_12_OFDM)
    {
        return DRV_RATE_12M;
    }

    if (uRateBitMap & DRV_RATE_MASK_11_CCK)
    {
        return DRV_RATE_11M;
    }

    if (uRateBitMap & DRV_RATE_MASK_9_OFDM)
    {
        return DRV_RATE_9M;
    }

    if (uRateBitMap & DRV_RATE_MASK_6_OFDM)
    {
        return DRV_RATE_6M;
    }

    if (uRateBitMap & DRV_RATE_MASK_5_5_CCK)
    {
        return DRV_RATE_5_5M;
    }

    if (uRateBitMap & DRV_RATE_MASK_2_BARKER)
    {
        return DRV_RATE_2M;
    }

    if (uRateBitMap & DRV_RATE_MASK_1_BARKER)
    {
        return DRV_RATE_1M;
    }

    return DRV_RATE_INVALID;
}

/************************************************************************
 *                        validateNetworkRate                           *
 ************************************************************************
DESCRIPTION: Verify that the input nitwork rate is valid
                                                                                                   
INPUT:      rate    -   input network rate

OUTPUT:     


RETURN:     TI_OK if valid, otherwise TI_NOK

************************************************************************/
static TI_STATUS rate_ValidateNet (ENetRate eRate)
{
    switch (eRate)
    {
        case NET_RATE_1M:
        case NET_RATE_1M_BASIC:
        case NET_RATE_2M:
        case NET_RATE_2M_BASIC:
        case NET_RATE_5_5M:
        case NET_RATE_5_5M_BASIC:
        case NET_RATE_11M:
        case NET_RATE_11M_BASIC:
        case NET_RATE_22M:
        case NET_RATE_22M_BASIC:
        case NET_RATE_6M:
        case NET_RATE_6M_BASIC:
        case NET_RATE_9M:
        case NET_RATE_9M_BASIC:
        case NET_RATE_12M:
        case NET_RATE_12M_BASIC:
        case NET_RATE_18M:
        case NET_RATE_18M_BASIC:
        case NET_RATE_24M:
        case NET_RATE_24M_BASIC:
        case NET_RATE_36M:
        case NET_RATE_36M_BASIC:
        case NET_RATE_48M:
        case NET_RATE_48M_BASIC:
        case NET_RATE_54M:
        case NET_RATE_54M_BASIC:
            return TI_OK;

        default:
            return TI_NOK;
    }
}

/************************************************************************
 *                        getMaxBasicRate                           *
 ************************************************************************
DESCRIPTION: Goes over an array of network rates and returns the max basic rate
                                                                                                   
INPUT:      pRates      -   Rate array

OUTPUT:     


RETURN:     Max basic rate (in network units)

************************************************************************/
ENetRate rate_GetMaxBasicFromStr (TI_UINT8 *pRatesString, TI_UINT32 len, ENetRate eMaxRate)
{
    TI_UINT32   i;
    
    for (i = 0; i < len; i++)
    {
        if (NET_BASIC_RATE (pRatesString[i]) && rate_ValidateNet ((ENetRate)pRatesString[i]) == TI_OK)
        {
            eMaxRate = TI_MAX ((ENetRate)pRatesString[i], eMaxRate);
        }
    }

    return eMaxRate;
}

/************************************************************************
 *                        getMaxActiveRate                          *
 ************************************************************************
DESCRIPTION: Goes over an array of network rates and returns the max active rate
                                                                                                   
INPUT:      pRates      -   Rate array

OUTPUT:     


RETURN:     Max active rate (in network units)

************************************************************************/
ENetRate rate_GetMaxActiveFromStr (TI_UINT8 *pRatesString, TI_UINT32 len, ENetRate eMaxRate)
{
    TI_UINT32   i;
    
    for (i = 0; i < len; i++)
    {
        if (NET_ACTIVE_RATE (pRatesString[i]) && rate_ValidateNet ((ENetRate)pRatesString[i]) == TI_OK)
        {
            eMaxRate = TI_MAX ((ENetRate)pRatesString[i], eMaxRate);
        }
    }

    return eMaxRate;
}

TI_UINT32 rate_DrvToNumber (ERate eRate)
{
    switch (eRate)
    {
        case DRV_RATE_1M:
            return 1;

        case DRV_RATE_2M:
            return 2;

        case DRV_RATE_5_5M:
            return 5;

        case DRV_RATE_11M:
            return 11;

        case DRV_RATE_22M:
            return 22;

        case DRV_RATE_6M:
            return 6;

        case DRV_RATE_9M:
            return 9;

        case DRV_RATE_12M:
            return 12;

        case DRV_RATE_18M:
            return 18;

        case DRV_RATE_24M:
            return 24;

        case DRV_RATE_36M:
            return 36;

        case DRV_RATE_48M:
            return 48;

        case DRV_RATE_54M:
            return 54;

        case DRV_RATE_MCS_0:
            return 6;
    
        case DRV_RATE_MCS_1:
            return 13;
    
        case DRV_RATE_MCS_2:
            return 19;
    
        case DRV_RATE_MCS_3:
            return 26;
    
        case DRV_RATE_MCS_4:
            return 39;
    
        case DRV_RATE_MCS_5:
            return 52;
    
        case DRV_RATE_MCS_6:
            return 58;
    
        case DRV_RATE_MCS_7:
            return 65;

        default:
            return 0;
    }
}

/************************************************************************
 *                        bitMapToNetworkStringRates                    *
 ************************************************************************
DESCRIPTION: Converts bit map to the rates string
                                                                                                   
INPUT:      suppRatesBitMap     -   bit map of supported rates
            basicRatesBitMap    -   bit map of basic rates

OUTPUT:     string - network format rates array,
            len - rates array length
            firstOFDMrateLoc - the index of first OFDM rate in the rates array.


RETURN:     None

************************************************************************/
TI_STATUS rate_DrvBitmapToNetStr (TI_UINT32   uSuppRatesBitMap,
                                  TI_UINT32   uBasicRatesBitMap,
                                  TI_UINT8    *string,
                                  TI_UINT32   *len,
                                  TI_UINT32   *pFirstOfdmRate)
{
    TI_UINT32   i = 0;
    
    if (uSuppRatesBitMap & DRV_RATE_MASK_1_BARKER)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_1_BARKER)
        {
            string[i++] = NET_RATE_1M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_1M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_2_BARKER)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_2_BARKER)
        {
            string[i++] = NET_RATE_2M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_2M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_5_5_CCK)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_5_5_CCK)
        {
            string[i++] = NET_RATE_5_5M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_5_5M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_11_CCK)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_11_CCK)
        {
            string[i++] = NET_RATE_11M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_11M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_22_PBCC)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_22_PBCC)
        {
            string[i++] = NET_RATE_22M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_22M;
        }
    }

    *pFirstOfdmRate = i;
    
    if (uSuppRatesBitMap & DRV_RATE_MASK_6_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_6_OFDM)
        {
            string[i++] = NET_RATE_6M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_6M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_9_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_9_OFDM)
        {
            string[i++] = NET_RATE_9M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_9M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_12_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_12_OFDM)
        {
            string[i++] = NET_RATE_12M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_12M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_18_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_18_OFDM)
        {
            string[i++] = NET_RATE_18M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_18M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_24_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_24_OFDM)
        {
            string[i++] = NET_RATE_24M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_24M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_36_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_36_OFDM)
        {
            string[i++] = NET_RATE_36M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_36M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_48_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_48_OFDM)
        {
            string[i++] = NET_RATE_48M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_48M;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_54_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_54_OFDM)
        {
            string[i++] = NET_RATE_54M_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_54M;
        }
    }

    *len = i;
    
    return TI_OK;
}


/************************************************************************
 *                        bitMapToNetworkStringRates                    *
 ************************************************************************
DESCRIPTION: Converts bit map to the rates string
                                                                                                   
INPUT:      suppRatesBitMap     -   bit map of supported rates
            basicRatesBitMap    -   bit map of basic rates

OUTPUT:     string - network format rates array,
            len - rates array length
            firstOFDMrateLoc - the index of first OFDM rate in the rates array.


RETURN:     None

************************************************************************/
TI_STATUS rate_DrvBitmapToNetStrIncluding11n (TI_UINT32   uSuppRatesBitMap,
											  TI_UINT32   uBasicRatesBitMap,
											  TI_UINT8    *string,
											  TI_UINT32   *pFirstOfdmRate)
{
    TI_UINT32   i = 0;


	rate_DrvBitmapToNetStr (uSuppRatesBitMap, uBasicRatesBitMap, string, &i, pFirstOfdmRate);

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_0_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_0_OFDM)
        {
            string[i++] = NET_RATE_MCS0_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS0;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_1_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_1_OFDM)
        {
            string[i++] = NET_RATE_MCS1_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS1;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_2_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_2_OFDM)
        {
            string[i++] = NET_RATE_MCS2_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS2;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_3_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_3_OFDM)
        {
            string[i++] = NET_RATE_MCS3_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS3;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_4_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_4_OFDM)
        {
            string[i++] = NET_RATE_MCS4_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS4;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_5_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_5_OFDM)
        {
            string[i++] = NET_RATE_MCS5_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS5;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_6_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_6_OFDM)
        {
            string[i++] = NET_RATE_MCS6_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS6;
        }
    }

    if (uSuppRatesBitMap & DRV_RATE_MASK_MCS_7_OFDM)
    {
        if (uBasicRatesBitMap & DRV_RATE_MASK_MCS_7_OFDM)
        {
            string[i++] = NET_RATE_MCS7_BASIC;
        }
        else
        {
            string[i++] = NET_RATE_MCS7;
        }
    }

    
    return TI_OK;
}

/************************************************************************
 *                        networkStringToBitMapSuppRates                *
 ************************************************************************
DESCRIPTION: Converts supported rates string to the bit map
                                                                                                   
INPUT:      string      -   array of rates in the network format
            len - array length

OUTPUT:     bitMap - bit map of rates.

RETURN:     None

************************************************************************/
TI_STATUS rate_NetStrToDrvBitmap (TI_UINT32 *pBitMap, TI_UINT8 *string, TI_UINT32 len)
{
    TI_UINT32   i;
    
    *pBitMap = 0;
    
    for (i = 0; i < len; i++)
    {
        switch (string[i])
        {
            case NET_RATE_1M:
            case NET_RATE_1M_BASIC:
                *pBitMap |= DRV_RATE_MASK_1_BARKER;
                break;

            case NET_RATE_2M:
            case NET_RATE_2M_BASIC:
                *pBitMap |= DRV_RATE_MASK_2_BARKER;
                break;

            case NET_RATE_5_5M:
            case NET_RATE_5_5M_BASIC:
                *pBitMap |= DRV_RATE_MASK_5_5_CCK;
                break;

            case NET_RATE_11M:
            case NET_RATE_11M_BASIC:
                *pBitMap |= DRV_RATE_MASK_11_CCK;
                break;

            case NET_RATE_22M:
            case NET_RATE_22M_BASIC:
                *pBitMap |= DRV_RATE_MASK_22_PBCC;
                break;

            case NET_RATE_6M:
            case NET_RATE_6M_BASIC:
                *pBitMap |= DRV_RATE_MASK_6_OFDM;
                break;

            case NET_RATE_9M:
            case NET_RATE_9M_BASIC:
                *pBitMap |= DRV_RATE_MASK_9_OFDM;
                break;

            case NET_RATE_12M:
            case NET_RATE_12M_BASIC:
                *pBitMap |= DRV_RATE_MASK_12_OFDM;
                break;

            case NET_RATE_18M:
            case NET_RATE_18M_BASIC:
                *pBitMap |= DRV_RATE_MASK_18_OFDM;
                break;

            case NET_RATE_24M:
            case NET_RATE_24M_BASIC:
                *pBitMap |= DRV_RATE_MASK_24_OFDM;
                break;

            case NET_RATE_36M:
            case NET_RATE_36M_BASIC:
                *pBitMap |= DRV_RATE_MASK_36_OFDM;
                break;

            case NET_RATE_48M:
            case NET_RATE_48M_BASIC:
                *pBitMap |= DRV_RATE_MASK_48_OFDM;
                break;

            case NET_RATE_54M:
            case NET_RATE_54M_BASIC:
                *pBitMap |= DRV_RATE_MASK_54_OFDM;
                break;

            case NET_RATE_MCS0:
            case NET_RATE_MCS0_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_0_OFDM;
                break;
    
            case NET_RATE_MCS1:
            case NET_RATE_MCS1_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_1_OFDM;
                break;
    
            case NET_RATE_MCS2:
            case NET_RATE_MCS2_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_2_OFDM;
                break;
    
            case NET_RATE_MCS3:
            case NET_RATE_MCS3_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_3_OFDM;
                break;
    
            case NET_RATE_MCS4:
            case NET_RATE_MCS4_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_4_OFDM;
                break;
    
            case NET_RATE_MCS5:
            case NET_RATE_MCS5_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_5_OFDM;
                break;
    
            case NET_RATE_MCS6:
            case NET_RATE_MCS6_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_6_OFDM;
                break;
    
            case NET_RATE_MCS7:
            case NET_RATE_MCS7_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_7_OFDM;
                break;

            default:
                break;
        }
    }

    return TI_OK;
}

/************************************************************************
 *                        networkStringToBitMapBasicRates               *
 ************************************************************************
DESCRIPTION: Converts basic rates string to the bit map
                                                                                                   
INPUT:      string      -   array of rates in the network format
            len - array length

OUTPUT:     bitMap - bit map of rates.

RETURN:     None

************************************************************************/
TI_STATUS rate_NetBasicStrToDrvBitmap (TI_UINT32 *pBitMap, TI_UINT8 *string, TI_UINT32 len)
{
    TI_UINT32   i;
    
    *pBitMap = 0;
    
    for (i = 0; i < len; i++)
    {
        switch (string[i])
        {
            case NET_RATE_1M_BASIC:
                *pBitMap |= DRV_RATE_MASK_1_BARKER;
                break;

            case NET_RATE_2M_BASIC:
                *pBitMap |= DRV_RATE_MASK_2_BARKER;
                break;

            case NET_RATE_5_5M_BASIC:
                *pBitMap |= DRV_RATE_MASK_5_5_CCK;
                break;

            case NET_RATE_11M_BASIC:
                *pBitMap |= DRV_RATE_MASK_11_CCK;
                break;

            case NET_RATE_22M_BASIC:
                *pBitMap |= DRV_RATE_MASK_22_PBCC;
                break;

            case NET_RATE_6M_BASIC:
                *pBitMap |= DRV_RATE_MASK_6_OFDM;
                break;

            case NET_RATE_9M_BASIC:
                *pBitMap |= DRV_RATE_MASK_9_OFDM;
                break;

            case NET_RATE_12M_BASIC:
                *pBitMap |= DRV_RATE_MASK_12_OFDM;
                break;

            case NET_RATE_18M_BASIC:
                *pBitMap |= DRV_RATE_MASK_18_OFDM;
                break;

            case NET_RATE_24M_BASIC:
                *pBitMap |= DRV_RATE_MASK_24_OFDM;
                break;

            case NET_RATE_36M_BASIC:
                *pBitMap |= DRV_RATE_MASK_36_OFDM;
                break;

            case NET_RATE_48M_BASIC:
                *pBitMap |= DRV_RATE_MASK_48_OFDM;
                break;

            case NET_RATE_54M_BASIC:
                *pBitMap |= DRV_RATE_MASK_54_OFDM;
                break;

            case NET_RATE_MCS0_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_0_OFDM;
                break;
    
            case NET_RATE_MCS1_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_1_OFDM;
                break;
    
            case NET_RATE_MCS2_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_2_OFDM;
                break;
    
            case NET_RATE_MCS3_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_3_OFDM;
                break;
    
            case NET_RATE_MCS4_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_4_OFDM;
                break;
    
            case NET_RATE_MCS5_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_5_OFDM;
                break;
    
            case NET_RATE_MCS6_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_6_OFDM;
                break;
    
            case NET_RATE_MCS7_BASIC:
                *pBitMap |= DRV_RATE_MASK_MCS_7_OFDM;
                break;
    
            default:
                break;
        }
    }

    return TI_OK;
}


/************************************************************************
 *                        rate_McsNetStrToDrvBitmap                     *
 ************************************************************************
DESCRIPTION: Converts MCS IEs rates bit map to driver bit map. 
             supported only MCS0 - MCS7 
                                                                                                   
INPUT:      string - HT capabilities IE in the network format
            len - IE array length

OUTPUT:     bitMap - bit map of rates.

RETURN:     None

************************************************************************/
TI_STATUS rate_McsNetStrToDrvBitmap (TI_UINT32 *pBitMap, TI_UINT8 *string)
{
    *pBitMap = string[0];

    *pBitMap = *pBitMap << (DRV_RATE_MCS_0 - 1);

    return TI_OK;
}


TI_STATUS rate_DrvBitmapToHwBitmap (TI_UINT32 uDrvBitMap, TI_UINT32 *pHwBitmap)
{
    TI_UINT32   uHwBitMap = 0;
    
    if (uDrvBitMap & DRV_RATE_MASK_1_BARKER)
    {
        uHwBitMap |= HW_BIT_RATE_1MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_2_BARKER)
    {
        uHwBitMap |= HW_BIT_RATE_2MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_5_5_CCK)
    {
        uHwBitMap |= HW_BIT_RATE_5_5MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_11_CCK)
    {
        uHwBitMap |= HW_BIT_RATE_11MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_22_PBCC)
    {
        uHwBitMap |= HW_BIT_RATE_22MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_6_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_6MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_9_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_9MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_12_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_12MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_18_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_18MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_24_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_24MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_36_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_36MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_48_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_48MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_54_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_54MBPS;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_0_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_0;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_1_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_1;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_2_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_2;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_3_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_3;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_4_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_4;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_5_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_5;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_6_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_6;
    }

    if (uDrvBitMap & DRV_RATE_MASK_MCS_7_OFDM)
    {
        uHwBitMap |= HW_BIT_RATE_MCS_7;
    }

    *pHwBitmap = uHwBitMap;
    
    return TI_OK;
}

TI_STATUS rate_PolicyToDrv (ETxRateClassId ePolicyRate, ERate *eAppRate)
{
    ERate     Rate = DRV_RATE_AUTO;
    TI_STATUS status = TI_OK;

    switch (ePolicyRate)
    {
        case txPolicy1    :   Rate =  DRV_RATE_1M   ;    break;
        case txPolicy2    :   Rate =  DRV_RATE_2M   ;    break;
        case txPolicy5_5  :   Rate =  DRV_RATE_5_5M ;    break;
        case txPolicy11   :   Rate =  DRV_RATE_11M  ;    break;
        case txPolicy22   :   Rate =  DRV_RATE_22M  ;    break;
        case txPolicy6    :   Rate =  DRV_RATE_6M   ;    break;
        case txPolicy9    :   Rate =  DRV_RATE_9M   ;    break;
        case txPolicy12   :   Rate =  DRV_RATE_12M  ;    break;
        case txPolicy18   :   Rate =  DRV_RATE_18M  ;    break;
        case txPolicy24   :   Rate =  DRV_RATE_24M  ;    break;
        case txPolicy36   :   Rate =  DRV_RATE_36M  ;    break;
        case txPolicy48   :   Rate =  DRV_RATE_48M  ;    break;
        case txPolicy54   :   Rate =  DRV_RATE_54M  ;    break;
        case txPolicyMcs0 :   Rate =  DRV_RATE_MCS_0;    break;
        case txPolicyMcs1 :   Rate =  DRV_RATE_MCS_1;    break;
        case txPolicyMcs2 :   Rate =  DRV_RATE_MCS_2;    break;
        case txPolicyMcs3 :   Rate =  DRV_RATE_MCS_3;    break;
        case txPolicyMcs4 :   Rate =  DRV_RATE_MCS_4;    break;
        case txPolicyMcs5 :   Rate =  DRV_RATE_MCS_5;    break;
        case txPolicyMcs6 :   Rate =  DRV_RATE_MCS_6;    break;
        case txPolicyMcs7 :   Rate =  DRV_RATE_MCS_7;    break;

        default:
            status = TI_NOK;
            break;
    }

    if (status == TI_OK)
        *eAppRate = Rate;
    else
        *eAppRate = DRV_RATE_INVALID; 

    return status;
}


TI_UINT32 rate_BasicToDrvBitmap (EBasicRateSet eBasicRateSet, TI_BOOL bDot11a)
{
    if (!bDot11a)
    {
        switch (eBasicRateSet)
        {
            case BASIC_RATE_SET_1_2:
                return DRV_RATE_MASK_1_BARKER | 
                       DRV_RATE_MASK_2_BARKER;

            case BASIC_RATE_SET_1_2_5_5_11:
                return DRV_RATE_MASK_1_BARKER | 
                       DRV_RATE_MASK_2_BARKER | 
                       DRV_RATE_MASK_5_5_CCK | 
                       DRV_RATE_MASK_11_CCK;

            case BASIC_RATE_SET_UP_TO_12:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM;

            case BASIC_RATE_SET_UP_TO_18:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM;

            case BASIC_RATE_SET_UP_TO_24:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM;

            case BASIC_RATE_SET_UP_TO_36:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM;

            case BASIC_RATE_SET_UP_TO_48:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM;

            case BASIC_RATE_SET_UP_TO_54:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;

            case BASIC_RATE_SET_6_12_24:
                return DRV_RATE_MASK_6_OFDM | 
                       DRV_RATE_MASK_12_OFDM | 
                       DRV_RATE_MASK_24_OFDM;

            case BASIC_RATE_SET_1_2_5_5_6_11_12_24:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_24_OFDM;

            case BASIC_RATE_SET_ALL_MCS_RATES:
                return DRV_RATE_MASK_MCS_0_OFDM |
                       DRV_RATE_MASK_MCS_1_OFDM |
                       DRV_RATE_MASK_MCS_2_OFDM |
                       DRV_RATE_MASK_MCS_3_OFDM |
                       DRV_RATE_MASK_MCS_4_OFDM |
                       DRV_RATE_MASK_MCS_5_OFDM |
                       DRV_RATE_MASK_MCS_6_OFDM |
                       DRV_RATE_MASK_MCS_7_OFDM |
                       DRV_RATE_MASK_1_BARKER   | 
                       DRV_RATE_MASK_2_BARKER   |
                       DRV_RATE_MASK_5_5_CCK    |  
                       DRV_RATE_MASK_11_CCK;


            default:
                return DRV_RATE_MASK_1_BARKER | 
                       DRV_RATE_MASK_2_BARKER;
        }
    }
    else
    {
        switch (eBasicRateSet)
        {
            case BASIC_RATE_SET_UP_TO_12:
                return DRV_RATE_MASK_6_OFDM | 
                       DRV_RATE_MASK_9_OFDM | 
                       DRV_RATE_MASK_12_OFDM;

            case BASIC_RATE_SET_UP_TO_18:
                return DRV_RATE_MASK_6_OFDM | 
                       DRV_RATE_MASK_9_OFDM | 
                       DRV_RATE_MASK_12_OFDM | 
                       DRV_RATE_MASK_18_OFDM;

            case BASIC_RATE_SET_UP_TO_24:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM;

            case BASIC_RATE_SET_UP_TO_36:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM;

            case BASIC_RATE_SET_UP_TO_48:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM;

            case BASIC_RATE_SET_UP_TO_54:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;

            case BASIC_RATE_SET_6_12_24:
                return DRV_RATE_MASK_6_OFDM | 
                       DRV_RATE_MASK_12_OFDM | 
                       DRV_RATE_MASK_24_OFDM;

            case BASIC_RATE_SET_ALL_MCS_RATES:
                return DRV_RATE_MASK_MCS_0_OFDM |
                       DRV_RATE_MASK_MCS_1_OFDM |
                       DRV_RATE_MASK_MCS_2_OFDM |
                       DRV_RATE_MASK_MCS_3_OFDM |
                       DRV_RATE_MASK_MCS_4_OFDM |
                       DRV_RATE_MASK_MCS_5_OFDM |
                       DRV_RATE_MASK_MCS_6_OFDM |
                       DRV_RATE_MASK_MCS_7_OFDM |
                       DRV_RATE_MASK_6_OFDM | 
                       DRV_RATE_MASK_12_OFDM | 
                       DRV_RATE_MASK_24_OFDM;

            default:
                return DRV_RATE_MASK_6_OFDM | 
                       DRV_RATE_MASK_12_OFDM | 
                       DRV_RATE_MASK_24_OFDM;
        }
    }
}

TI_UINT32 rate_SupportedToDrvBitmap (ESupportedRateSet eSupportedRateSet, TI_BOOL bDot11a)
{
    if (!bDot11a)
    {
        switch (eSupportedRateSet)
        {
            case SUPPORTED_RATE_SET_1_2:
                return DRV_RATE_MASK_1_BARKER | 
                       DRV_RATE_MASK_2_BARKER;

            case SUPPORTED_RATE_SET_1_2_5_5_11:
                return DRV_RATE_MASK_1_BARKER | 
                       DRV_RATE_MASK_2_BARKER | 
                       DRV_RATE_MASK_5_5_CCK | 
                       DRV_RATE_MASK_11_CCK;

            case SUPPORTED_RATE_SET_1_2_5_5_11_22:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_22_PBCC;

            case SUPPORTED_RATE_SET_UP_TO_18:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_24:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_36:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_48:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_54:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;

            case SUPPORTED_RATE_SET_ALL:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_22_PBCC |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;

            case SUPPORTED_RATE_SET_ALL_OFDM:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;
                       
            case SUPPORTED_RATE_SET_ALL_MCS_RATES:
                return DRV_RATE_MASK_MCS_0_OFDM |
                       DRV_RATE_MASK_MCS_1_OFDM |
                       DRV_RATE_MASK_MCS_2_OFDM |
                       DRV_RATE_MASK_MCS_3_OFDM |
                       DRV_RATE_MASK_MCS_4_OFDM |
                       DRV_RATE_MASK_MCS_5_OFDM |
                       DRV_RATE_MASK_MCS_6_OFDM |
                       DRV_RATE_MASK_MCS_7_OFDM |
                       DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_22_PBCC |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;

            default:
                return DRV_RATE_MASK_1_BARKER |
                       DRV_RATE_MASK_2_BARKER |
                       DRV_RATE_MASK_5_5_CCK |
                       DRV_RATE_MASK_11_CCK |
                       DRV_RATE_MASK_22_PBCC |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;
        }
    }
    else
    {
        switch (eSupportedRateSet)
        {
            case SUPPORTED_RATE_SET_UP_TO_18:
                return DRV_RATE_MASK_6_OFDM | 
                       DRV_RATE_MASK_9_OFDM | 
                       DRV_RATE_MASK_12_OFDM | 
                       DRV_RATE_MASK_18_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_24:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_36:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_48:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM;

            case SUPPORTED_RATE_SET_UP_TO_54:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;
                       
            case SUPPORTED_RATE_SET_ALL:
            case SUPPORTED_RATE_SET_ALL_OFDM:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;
                       
            case SUPPORTED_RATE_SET_ALL_MCS_RATES:
                return DRV_RATE_MASK_MCS_0_OFDM |
                       DRV_RATE_MASK_MCS_1_OFDM |
                       DRV_RATE_MASK_MCS_2_OFDM |
                       DRV_RATE_MASK_MCS_3_OFDM |
                       DRV_RATE_MASK_MCS_4_OFDM |
                       DRV_RATE_MASK_MCS_5_OFDM |
                       DRV_RATE_MASK_MCS_6_OFDM |
                       DRV_RATE_MASK_MCS_7_OFDM |
                       DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;

            default:
                return DRV_RATE_MASK_6_OFDM |
                       DRV_RATE_MASK_9_OFDM |
                       DRV_RATE_MASK_12_OFDM |
                       DRV_RATE_MASK_18_OFDM |
                       DRV_RATE_MASK_24_OFDM |
                       DRV_RATE_MASK_36_OFDM |
                       DRV_RATE_MASK_48_OFDM |
                       DRV_RATE_MASK_54_OFDM;
        }
    }
}

TI_STATUS rate_ValidateVsBand (TI_UINT32 *pSupportedMask, TI_UINT32 *pBasicMask, TI_BOOL bDot11a)
{
    if (bDot11a)
    {
        *pSupportedMask &= ~
            (
                DRV_RATE_MASK_1_BARKER |
                DRV_RATE_MASK_2_BARKER |
                DRV_RATE_MASK_5_5_CCK |
                DRV_RATE_MASK_11_CCK |
                DRV_RATE_MASK_22_PBCC
            );
    }

    *pBasicMask &= *pSupportedMask;

    if (*pBasicMask == 0)
    {
        if (bDot11a)
        {
            *pBasicMask = DRV_RATE_MASK_6_OFDM | DRV_RATE_MASK_12_OFDM | DRV_RATE_MASK_24_OFDM;
        }
        else
        {
            *pBasicMask = DRV_RATE_MASK_1_BARKER | DRV_RATE_MASK_2_BARKER;
        }
    }

    return TI_OK;
}

/*-----------------------------------------------------------------------------
Routine Name:    RateNumberToHost
Routine Description:
Arguments:
Return Value:    None
-----------------------------------------------------------------------------*/
ERate rate_NumberToDrv (TI_UINT32 rate)
{
    switch (rate)
    {
        case 0x1:
            return DRV_RATE_1M;

        case 0x2:
            return DRV_RATE_2M;

        case 0x5:
            return DRV_RATE_5_5M;

        case 0xB:
            return DRV_RATE_11M;

        case 0x16:
            return DRV_RATE_22M;

        case 0x6:
            return DRV_RATE_6M;

        case 0x9:
            return DRV_RATE_9M;

        case 0xC:
            return DRV_RATE_12M;

        case 0x12:
            return DRV_RATE_18M;

        case 0x18:
            return DRV_RATE_24M;

        case 0x24:
            return DRV_RATE_36M;

        case 0x30:
            return DRV_RATE_48M;

        case 0x36:
            return DRV_RATE_54M;

        /* MCS rate */
        case 0x7:
            return DRV_RATE_MCS_0;

        case 0xD:
            return DRV_RATE_MCS_1;

        case 0x13:
            return DRV_RATE_MCS_2;

        case 0x1A:
            return DRV_RATE_MCS_3;

        case 0x27:
            return DRV_RATE_MCS_4;

        case 0x34:
            return DRV_RATE_MCS_5;

        case 0x3A:
            return DRV_RATE_MCS_6;

        case 0x41:
            return DRV_RATE_MCS_7;

        default:
            return DRV_RATE_6M;
    }
}

TI_UINT32 rate_GetDrvBitmapForDefaultBasicSet ()
{
    return rate_BasicToDrvBitmap (BASIC_RATE_SET_1_2_5_5_11, TI_FALSE);
}

TI_UINT32 rate_GetDrvBitmapForDefaultSupporteSet ()
{
    return rate_SupportedToDrvBitmap (SUPPORTED_RATE_SET_1_2_5_5_11, TI_FALSE);
}

