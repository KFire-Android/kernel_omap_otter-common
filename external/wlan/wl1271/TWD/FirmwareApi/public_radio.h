/*
 * public_radio.h
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

/**********************************************************************************************************************
  FILENAME:       public_radio.h

  DESCRIPTION:    Contains information element defines/structures used by the TNETxxxx and host and Radio Module.
                  This is a PUBLIC header, which customers will use.
***********************************************************************************************************************/
/*
=======================================================================================================================
                      R E V I S I O N    H I S T O R Y

  04/29/05  BRK  1. retrieved from ClearCase and added this rev. history
                 2. added two new entries to RadioParamType_e  enum
                 3. increased MAX_RADIO_PARAM_POWER_TABLE (from 20 to 56)
                    - this is sort of a kludge, struct RadioParam_t  should have used an
                      array pointer instead of an actual data block
  06/10/05  BRK  changed MAX_RADIO_PARAM_POWER_TABLE for 1251 support (sort of a KLUDGE)
  07/15/05  BRK  added RADIO_PABIAS_TABLE entry to RadioParamType_e  enum
  04/12/06  MH   Added new run-time calibration state: RFPLL_CALIBRATION_NEEDED

  Note: This code should only be edited with TAB stops set at 4
=======================================================================================================================
 */
#ifndef PUBLIC_RADIO
#define PUBLIC_RADIO

#include "public_types.h"
#define MAC_ADDR_SIZE 6
/* typedef uint8 TMacAddr[MAC_ADDR_SIZE]; */
/*defined in tiDefs.h*/
/************************************************************************/
/*																		*/	
/*							Definitions section                         */
/*																		*/
/************************************************************************/
/* radio parameter to set */
#ifdef TNETW1251
#define MAX_RADIO_PARAM_POWER_TABLE  			(4*48)		/* cPowLmtTbl[] max size for ABG radios */
#else
#define MAX_RADIO_PARAM_POWER_TABLE  			(56)     	/* cPowLmtTbl[] max size for BG radios*/
#endif
#define MAX_RADIO_PARAM_LEN          			(MAX_RADIO_PARAM_POWER_TABLE)

#define RADIO_PARAM_POWER_TABLE_ENABLE        	(0x01)  	/* mask for RADIO_PARAM_POWER_ENABLES usage*/
#define RADIO_PARAM_POWER_LIMIT_TABLE_ENABLE  	(0x02)  	/* mask for RADIO_PARAM_POWER_ENABLES usage*/
#define RADIO_PARAM_POWER_ADJ_TABLE_ENABLE    	(0x04)  	/* mask for RADIO_PARAM_POWER_ENABLES usage*/

#define NUM_OF_POWER_LEVEL      				(4)


#define TX_TEMPLATE_MAX_BUF_LEN					(512)

#define RX_PLT_LNA_STEPS_BUF_LEN				(4)
#define RX_PLT_TA_STEPS_BUF_LEN					(4)

#define RX_STAT_PACKETS_PER_MESSAGE           	(20) 

#define MULTIPLE_PACKET_SIZE		1024
#define MAX_TX_PACKET_SIZE_11_B		(2 * MULTIPLE_PACKET_SIZE)
#define MAX_TX_PACKET_SIZE_11_G		(4 * MULTIPLE_PACKET_SIZE)
#define MAX_TX_PACKET_SIZE_11_N		(8 * MULTIPLE_PACKET_SIZE) /* must be PDU */

/* Radio Band */
typedef enum
{
	eELEVEN_A_B,
	eELEVEN_A_G,
	eELEVEN_N,

	MAX_MODULATION
}Modulation;

/************************************************************************/
/*																		*/	
/*							Enumerations section                        */
/*																		*/
/************************************************************************/

/* Radio band types. */
typedef enum RADIO_BAND_TYPE_ENMT
{		
	FIRST_BAND_TYPE_E,
/*______________________________________*/

	_2_4_G_BAND_TYPE_E = FIRST_BAND_TYPE_E,
	_5_G_BAND_TYPE_E,
/*_______________________________________________*/
    UNUSED_BAND_TYPE_E,
	NUMBER_OF_BANDS_E = UNUSED_BAND_TYPE_E,				
	LAST_BAND_TYPE_E = (NUMBER_OF_BANDS_E - 1)

}RADIO_BAND_TYPE_ENM;


#define RADIO_BAND_2_4GHZ_BASE_FREQUENCY					2407
#define RADIO_BAND_JAPAN_4_9_GHZ_BASE_FREQUENCY				5000
#define RADIO_BAND_5GHZ_BASE_FREQUENCY						5000

#define RADIO_BAND_2_4GHZ_MULTIPLE_BASE_FREQUENCY			5
#define RADIO_BAND_JAPAN_4_9_GHZ_MULTIPLE_BASE_FREQUENCY	(-5)
#define RADIO_BAND_5GHZ_MULTIPLE_BASE_FREQUENCY				5

#define GIGA_HZ_TO_MEGA_HZ									1000



/* Radio sub-band types. */
typedef enum RADIO_SUB_BAND_TYPE_ENMT
{
	FIRST_SUB_BAND_TYPE_E,
/*______________________________________*/

	_2_4_G_SUB_BAND_TYPE_E = FIRST_SUB_BAND_TYPE_E, /* band b/g */
	FIRST_SUB_BANDS_IN_5G_BAND_E,
	LOW_JAPAN_4_9_G_SUB_BAND_TYPE_E = FIRST_SUB_BANDS_IN_5G_BAND_E,	/* band 4.9Ghz (Japan) low sub-band (J1-J4) */
	MID_JAPAN_4_9_G_SUB_BAND_TYPE_E,                /* band 4.9Ghz (Japan) mid sub-band(J8,J12,J16) */
	HIGH_JAPAN_4_9_G_SUB_BAND_TYPE_E,               /* band 4.9Ghz (Japan) high sub-band(J34,36,J38,40, J42, 44, J46,48) */
	_5_G_FIRST_SUB_BAND_TYPE_E,                     /* band 5GHz 1st sub-band(52->64 in steps of 4) */
	_5_G_SECOND_SUB_BAND_TYPE_E,                    /* band 5GHz 2nd sub-band(100->116 in steps of 4) */
	_5_G_THIRD_SUB_BAND_TYPE_E,                     /* band 5GHz 3rd sub-band(120->140 in steps of 4) */
    _5_G_FOURTH_SUB_BAND_TYPE_E,                    /* band 5GHz 4th sub-band(149->165 in steps of 4) */
	LAST_SUB_BANDS_IN_5G_BAND_E = _5_G_FOURTH_SUB_BAND_TYPE_E,
/*_______________________________________________*/
    UNUSED_SUB_BAND_TYPE_E,
	NUMBER_OF_SUB_BANDS_E = UNUSED_SUB_BAND_TYPE_E,				
	LAST_SUB_BAND_TYPE_E = (NUMBER_OF_SUB_BANDS_E - 1)

}RADIO_SUB_BAND_TYPE_ENM;

#define NUMBER_OF_SUB_BANDS_IN_5G_BAND_E	(LAST_SUB_BANDS_IN_5G_BAND_E - FIRST_SUB_BANDS_IN_5G_BAND_E + 1)	

typedef struct
{
	uint8					uDbm[NUMBER_OF_SUB_BANDS_E][NUM_OF_POWER_LEVEL]; 
} TpowerLevelTable_t;

/* Channel number */
typedef enum RADIO_CHANNEL_NUMBER_ENMT
{
	/*---------------------------------*/
	/* _2_4_G_SUB_BAND_TYPE_E          */
	/*---------------------------------*/
	
	/* index 0 */ RADIO_CHANNEL_INDEX_0_NUMBER_1_E = 1,
	/* index 1 */ RADIO_CHANNEL_INDEX_1_NUMBER_2_E = 2,	
	/* index 2 */ RADIO_CHANNEL_INDEX_2_NUMBER_3_E = 3,	
	/* index 3 */ RADIO_CHANNEL_INDEX_3_NUMBER_4_E = 4,	
	/* index 4 */ RADIO_CHANNEL_INDEX_4_NUMBER_5_E = 5,	
	/* index 5 */ RADIO_CHANNEL_INDEX_5_NUMBER_6_E = 6,	
	/* index 6 */ RADIO_CHANNEL_INDEX_6_NUMBER_7_E = 7,	
	/* index 7 */ RADIO_CHANNEL_INDEX_7_NUMBER_8_E = 8,	
	/* index 8 */ RADIO_CHANNEL_INDEX_8_NUMBER_9_E = 9,	
	/* index 9 */ RADIO_CHANNEL_INDEX_9_NUMBER_10_E = 10,	
	/* index 10 */ RADIO_CHANNEL_INDEX_10_NUMBER_11_E = 11,	
	/* index 11 */ RADIO_CHANNEL_INDEX_11_NUMBER_12_E = 12,	
	/* index 12 */ RADIO_CHANNEL_INDEX_12_NUMBER_13_E = 13,	
	/* index 13 */ RADIO_CHANNEL_INDEX_13_NUMBER_14_E = 14,	
	
	/*---------------------------------*/
	/* LOW_JAPAN_4_9_G_SUB_BAND_TYPE_E */
	/*---------------------------------*/
	/* index 14 */ RADIO_CHANNEL_INDEX_14_NUMBER_J1_E = 16,	
	/* index 15 */ RADIO_CHANNEL_INDEX_15_NUMBER_J2_E = 12,	
	/* index 16 */ RADIO_CHANNEL_INDEX_16_NUMBER_J3_E = 8,		
	/* index 17 */ RADIO_CHANNEL_INDEX_17_NUMBER_J4_E = 4,	
    
	/*---------------------------------*/
	/* MID_JAPAN_4_9_G_SUB_BAND_TYPE_E */
	/*---------------------------------*/
	/* index 18 */ RADIO_CHANNEL_INDEX_18_NUMBER_J8_E = 8,
	/* index 19 */ RADIO_CHANNEL_INDEX_19_NUMBER_J12_E = 12,
	/* index 20 */ RADIO_CHANNEL_INDEX_20_NUMBER_J16_E = 16,

	/*----------------------------------*/
	/* HIGH_JAPAN_4_9_G_SUB_BAND_TYPE_E */
	/*----------------------------------*/
	/* index 21 */ RADIO_CHANNEL_INDEX_21_NUMBER_J34_E = 34,
	/* index 22 */ RADIO_CHANNEL_INDEX_22_NUMBER_36_E = 36,
	/* index 23 */ RADIO_CHANNEL_INDEX_23_NUMBER_J38_E = 38,
	/* index 24 */ RADIO_CHANNEL_INDEX_24_NUMBER_40_E = 40,
	/* index 25 */ RADIO_CHANNEL_INDEX_25_NUMBER_J42_E = 42,
	/* index 26 */ RADIO_CHANNEL_INDEX_26_NUMBER_44_E = 44,
	/* index 27 */ RADIO_CHANNEL_INDEX_27_NUMBER_J46_E = 46,
	/* index 28 */ RADIO_CHANNEL_INDEX_28_NUMBER_48_E = 48, 

	/*---------------------------------*/
	/* _5_G_FIRST_SUB_BAND_TYPE_E      */
	/*---------------------------------*/
	/* index 29 */ RADIO_CHANNEL_INDEX_29_NUMBER_52_E = 52,
	/* index 30 */ RADIO_CHANNEL_INDEX_30_NUMBER_56_E = 56,
	/* index 31 */ RADIO_CHANNEL_INDEX_31_ENUMBER_60_E = 60,
	/* index 32 */ RADIO_CHANNEL_INDEX_32_ENUMBER_64_E = 64,

	/*---------------------------------*/
	/* _5_G_SECOND_SUB_BAND_TYPE_E     */
	/*---------------------------------*/
	/* index 33 */ RADIO_CHANNEL_INDEX_33_NUMBER_100_E = 100,
	/* index 34 */ RADIO_CHANNEL_INDEX_34_NUMBER_104_E = 104,
	/* index 35 */ RADIO_CHANNEL_INDEX_35_NUMBER_108_E = 108,
	/* index 36 */ RADIO_CHANNEL_INDEX_36_NUMBER_112_E = 112,
	/* index 37 */ RADIO_CHANNEL_INDEX_37_NUMBER_116_E = 116,

	/*---------------------------------*/
	/* _5_G_THIRD_SUB_BAND_TYPE_E      */
	/*---------------------------------*/
	/* index 38 */ RADIO_CHANNEL_INDEX_38_NUMBER_120_E = 120,
	/* index 39 */ RADIO_CHANNEL_INDEX_39_NUMBER_124_E = 124,
	/* index 40 */ RADIO_CHANNEL_INDEX_40_NUMBER_128_E = 128,
	/* index 41 */ RADIO_CHANNEL_INDEX_41_NUMBER_132_E = 132,
	/* index 42 */ RADIO_CHANNEL_INDEX_42_NUMBER_136_E = 136,
	/* index 43 */ RADIO_CHANNEL_INDEX_43_NUMBER_140_E = 140,


	/*---------------------------------*/
	/* _5_G_FOURTH_SUB_BAND_TYPE_E     */
	/*---------------------------------*/
	/* index 44 */ RADIO_CHANNEL_INDEX_44_NUMBER_149_E = 149,
	/* index 45 */ RADIO_CHANNEL_INDEX_45_NUMBER_153_E = 153,
	/* index 46 */ RADIO_CHANNEL_INDEX_46_NUMBER_157_E = 157,
	/* index 47 */ RADIO_CHANNEL_INDEX_47_NUMBER_161_E = 161,
	/* index 48 */ RADIO_CHANNEL_INDEX_48_NUMBER_165_E = 165
    
}RADIO_CHANNEL_NUMBER_ENM;

/* Radio channels */
typedef enum RADIO_CHANNEL_INDEX_ENMT
{		
	FIRST_RADIO_CHANNEL_INDEX_E,
/*______________________________________*/

	/*---------------------------------*/
	/* _2_4_G_SUB_BAND_TYPE_E          */
	/*---------------------------------*/
	FIRST_2_4_G_BAND_RADIO_CHANNEL_INDEX_E = FIRST_RADIO_CHANNEL_INDEX_E,					/* 0 */
	FIRST_2_4_G_SUB_BAND_RADIO_CHANNEL_INDEX_E = FIRST_2_4_G_BAND_RADIO_CHANNEL_INDEX_E,	/* 0 */
	
	/* Channels 0-13 indexes in the FW are 1-14 channels number in the RS */
	RADIO_CHANNEL_INDEX_0_E = FIRST_2_4_G_SUB_BAND_RADIO_CHANNEL_INDEX_E,/* 0 */
	RADIO_CHANNEL_INDEX_1_E,	/* 1 */
	RADIO_CHANNEL_INDEX_2_E,	/* 2 */
	RADIO_CHANNEL_INDEX_3_E,	/* 3 */
	RADIO_CHANNEL_INDEX_4_E,	/* 4 */
	RADIO_CHANNEL_INDEX_5_E,	/* 5 */
	RADIO_CHANNEL_INDEX_6_E,	/* 6 */
	RADIO_CHANNEL_INDEX_7_E,	/* 7 */
	RADIO_CHANNEL_INDEX_8_E,	/* 8 */
	RADIO_CHANNEL_INDEX_9_E,	/* 9 */
	RADIO_CHANNEL_INDEX_10_E,	/* 10 */
	RADIO_CHANNEL_INDEX_11_E,	/* 11 */
	RADIO_CHANNEL_INDEX_12_E,	/* 12 */
	RADIO_CHANNEL_INDEX_13_E,	/* 13 */	
	LAST_2_4_G_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_13_E,				/* 13 */
    LAST_2_4_G_BAND_RADIO_CHANNEL_INDEX_E = LAST_2_4_G_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 13 */
	NUMBER_OF_2_4_G_CHANNEL_INDICES_E = LAST_2_4_G_BAND_RADIO_CHANNEL_INDEX_E,			/* 13 */

	/*---------------------------------*/
	/* LOW_JAPAN_4_9_G_SUB_BAND_TYPE_E */
	/*---------------------------------*/
	FIRST_5_G_BAND_RADIO_CHANNEL_INDEX_E,	/* 14 */
	FIRST_LOW_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E = FIRST_5_G_BAND_RADIO_CHANNEL_INDEX_E,	/* 14 */

	/* Channels 14-17 indexes in the FW are J1-J4 channels number in the RS */
	RADIO_CHANNEL_INDEX_14_E = FIRST_LOW_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E,				/* 14 */
	RADIO_CHANNEL_INDEX_15_E,	/* 15 */
	RADIO_CHANNEL_INDEX_16_E,	/* 16 */
	RADIO_CHANNEL_INDEX_17_E,	/* 17 */
    LAST_LOW_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_17_E,	/* 17 */
	
	/*---------------------------------*/
	/* MID_JAPAN_4_9_G_SUB_BAND_TYPE_E */
	/*---------------------------------*/
	FIRST_MID_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 18 */

	/* Channel 18 index in the FW is channel number J8 in the RS */
    RADIO_CHANNEL_INDEX_18_E = FIRST_MID_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 18 */ 

	/* Channel 19 index in the FW is channel number J12 in the RS */
	RADIO_CHANNEL_INDEX_19_E,	/* 19 */
	
	/* Channel 20 index in the FW is channel number J16 in the RS */
	RADIO_CHANNEL_INDEX_20_E,	/* 20 */
    LAST_MID_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_20_E,		/* 20 */
    
	/*----------------------------------*/
	/* HIGH_JAPAN_4_9_G_SUB_BAND_TYPE_E */
	/*----------------------------------*/
	FIRST_HIGH_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 21 */

	/* Channel 21 index in the FW is channel number J34 in the RS */
    RADIO_CHANNEL_INDEX_21_E = FIRST_HIGH_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E, /* 21 */

	/* Channel 22 index in the FW is channel number 36 in the RS */
	RADIO_CHANNEL_INDEX_22_E,	/* 22 */

	/* Channel 23 index in the FW is channel number J38 in the RS */
	RADIO_CHANNEL_INDEX_23_E,	/* 23 */

	/* Channel 24 index in the FW is channel number 40 in the RS */
	RADIO_CHANNEL_INDEX_24_E,	/* 24 */

	/* Channel 25 index in the FW is channel number J42 in the RS */
	RADIO_CHANNEL_INDEX_25_E,	/* 25 */

	/* Channel 26 index in the FW is channel number 44 in the RS */
	RADIO_CHANNEL_INDEX_26_E,	/* 26 */

	/* Channel 27 index in the FW is channel number J46 in the RS */
	RADIO_CHANNEL_INDEX_27_E,	/* 27 */

	/* Channel 28 index in the FW is channel number 48 in the RS */
	RADIO_CHANNEL_INDEX_28_E,	/* 28 */
    LAST_HIGH_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_28_E, /* 28 */
    LAST_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E = LAST_HIGH_JAPAN_4_9_G_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 28 */
	/*---------------------------------*/
	/* _5_G_FIRST_SUB_BAND_TYPE_E      */
	/*---------------------------------*/
	FIRST_5_G_FIRST_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 29 */

	/* Channel 29 index in the FW is channel number 52 in the RS */
	RADIO_CHANNEL_INDEX_29_E = FIRST_5_G_FIRST_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 29 */

	/* Channel 30 index in the FW is channel number 56 in the RS */
	RADIO_CHANNEL_INDEX_30_E,	/* 30 */

	/* Channel 31 index in the FW is channel number 60 in the RS */
	RADIO_CHANNEL_INDEX_31_E,	/* 31 */

	/* Channel 32 index in the FW is channel number 64 in the RS */
	RADIO_CHANNEL_INDEX_32_E,	/* 32 */
	LAST_5_G_FIRST_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_32_E,	/* 32 */
	
	/*---------------------------------*/
	/* _5_G_SECOND_SUB_BAND_TYPE_E     */
	/*---------------------------------*/
	FIRST_5_G_SECOND_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 33 */

	/* Channel 33 index in the FW is channel number 100 in the RS */
	RADIO_CHANNEL_INDEX_33_E = FIRST_5_G_SECOND_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 33 */

	/* Channel 34 index in the FW is channel number 104 in the RS */
	RADIO_CHANNEL_INDEX_34_E,	/* 34 */

	/* Channel 35 index in the FW is channel number 108 in the RS */
	RADIO_CHANNEL_INDEX_35_E,	/* 35 */

	/* Channel 36 index in the FW is channel number 112 in the RS */
	RADIO_CHANNEL_INDEX_36_E,	/* 36 */

	/* Channel 37 index in the FW is channel number 116 in the RS */
	RADIO_CHANNEL_INDEX_37_E,	/* 37 */
	LAST_5_G_SECOND_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_37_E,	/* 37 */
	
	/*---------------------------------*/
	/* _5_G_THIRD_SUB_BAND_TYPE_E      */
	/*---------------------------------*/
	FIRST_5_G_THIRD_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 38 */

	/* Channel 38 index in the FW is channel number 120 in the RS */
	RADIO_CHANNEL_INDEX_38_E = FIRST_5_G_THIRD_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 38 */

	/* Channel 39 index in the FW is channel number 124 in the RS */
	RADIO_CHANNEL_INDEX_39_E,	/* 39 */

	/* Channel 40 index in the FW in the FW is channel number 128 in the RS */
	RADIO_CHANNEL_INDEX_40_E,	/* 40 */

	/* Channel 41 index in the FW is channel number 132 in the RS */
	RADIO_CHANNEL_INDEX_41_E,	/* 41 */

	/* Channel 42 index in the FW is channel number 136 in the RS */
	RADIO_CHANNEL_INDEX_42_E,	/* 42 */

	/* Channel 43 index in the FW is channel number 140 in the RS */
	RADIO_CHANNEL_INDEX_43_E,	/* 43 */
	LAST_5_G_THIRD_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_43_E,	/* 43 */
	
	/*---------------------------------*/
	/* _5_G_FOURTH_SUB_BAND_TYPE_E     */
	/*---------------------------------*/
	FIRST_5_G_FOURTH_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 44 */

	/* Channel 44 index in the FW is channel number 149 in the RS */
    RADIO_CHANNEL_INDEX_44_E = FIRST_5_G_FOURTH_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 44 */

	/* Channel 45 index in the FW is channel number 153 in the RS */
	RADIO_CHANNEL_INDEX_45_E,	/* 45 */

	/* Channel 46 index in the FW is channel number 157 in the RS */
	RADIO_CHANNEL_INDEX_46_E,	/* 46 */

	/* Channel 47 index in the FW is channel number 161 in the RS */
	RADIO_CHANNEL_INDEX_47_E,	/* 47 */

	/* Channel 48 index in the FW is channel number 165 in the RS */
	RADIO_CHANNEL_INDEX_48_E,	/* 48 */
	LAST_5_G_FOURTH_SUB_BAND_RADIO_CHANNEL_INDEX_E = RADIO_CHANNEL_INDEX_48_E,	/* 48 */
	LAST_5_G_BAND_RADIO_CHANNEL_INDEX_E = LAST_5_G_FOURTH_SUB_BAND_RADIO_CHANNEL_INDEX_E,	/* 48 */	
/*_______________________________________________*/

    UNUSED_RADIO_CHANNEL_INDEX_E,               /* 49 */
	NUMBER_OF_RADIO_CHANNEL_INDEXS_E = UNUSED_RADIO_CHANNEL_INDEX_E,	/* 49 */
	LAST_RADIO_CHANNEL_INDEX_E = (NUMBER_OF_RADIO_CHANNEL_INDEXS_E - 1)	/* 48 */

}RADIO_CHANNEL_INDEX_ENM;

#define NUMBER_OF_2_4_G_CHANNELS    	(NUMBER_OF_2_4_G_CHANNEL_INDICES_E + 1)
#define NUMBER_OF_5G_CHANNELS       	(NUMBER_OF_RADIO_CHANNEL_INDEXS_E - NUMBER_OF_2_4_G_CHANNELS)
#define HALF_NUMBER_OF_2_4_G_CHANNELS 	(NUMBER_OF_2_4_G_CHANNELS / 2)
#define HALF_NUMBER_OF_5G_CHANNELS  	((NUMBER_OF_5G_CHANNELS + 1) / 2)

typedef enum RADIO_RATE_GROUPS_ENMT
{		
	FIRST_RATE_GROUP_E,
/*______________________________________*/

	MCS7_RATE_GROUP_E = FIRST_RATE_GROUP_E,
	_54_48_RATE_GROUP_E,                /* band 4.9Ghz (Japan) low sub-band (J1-J4) */
	_36_24_RATE_GROUP_E,                /* band 4.9Ghz (Japan) mid sub-band(J8,J12,J16) */
	_18_12_RATE_GROUP_E,                /* band 4.9Ghz (Japan) high sub-band(J34,36,J38,40, J42, 44, J46,48) */
	_9_6_RATE_GROUP_E,                  /* band 5GHz 1st sub-band(52->64 in steps of 4) */
	_11b_RATE_GROUP_E,                  /* band 5GHz 2nd sub-band(100->116 in steps of 4) */
/*_______________________________________________*/
    UNUSED_RATE_GROUPS_E,
	NUMBER_OF_RATE_GROUPS_E = UNUSED_RATE_GROUPS_E,				
	LAST_RATE_GROUP_E = (NUMBER_OF_RATE_GROUPS_E - 1)

}RADIO_RATE_GROUPS_ENM;


typedef enum
{
    RADIO_BAND_2_4_GHZ                  = 0,
    RADIO_BAND_5_0_GHZ                  = 1,
    RADIO_BAND_DUAL                     = 2,
    RADIO_BAND_NUM_OF_BANDS             = 2

} ERadioBand;


/******************************************************************************
 TTestCmdRunCalibration - Calibration manager message
                  
 Note:  
******************************************************************************/

typedef enum CALIBRATION_COMMANDS_ENMT
{
	/* RX */
	CM_space1_e,
	CM_RX_IQ_MM_calibration_e,
	CM_RX_IQ_MM_correction_upon_channel_change_e,
	CM_RX_IQ_MM_correction_upon_temperature_change_e,
	CM_RX_IQ_MM_duplicate_VGA_e,
	CM_space2_e,

	CM_RX_analog_DC_Correction_calibration_e,
	CM_RX_DC_AUX_cal_mode_e,
	CM_RX_DC_AUX_normal_mode_e,
	CM_space3_e,

	CM_RX_BIP_enter_mode_e,
	CM_RX_BIP_perform_e,
	CM_RX_BIP_exit_mode_e,
	CM_space4_e,

	/* TX */
	CM_TX_power_detector_calibration_e,
	CM_TX_power_detector_buffer_calibration_e,
	CM_space5_e,

	CM_TX_LO_Leakage_calibration_e,
	CM_TX_PPA_Steps_calibration_e,
	CM_TX_CLPC_calibration_e,
	CM_TX_IQ_MM_calibration_e,
	CM_TX_BIP_calibration_e,
    /* DRPw */
	CM_RX_TANK_TUNE_calibration_e,
/*    CM_PD_BUFF_TUNE_calibration_e,*/
    CM_RX_DAC_TUNE_calibration_e,
    CM_RX_IQMM_TUNE_calibration_e,
    CM_RX_LPF_TUNE_calibration_e,
    CM_TX_LPF_TUNE_calibration_e,
    CM_TA_TUNE_calibration_e,
    CM_TX_MIXERFREQ_calibration_e,
    CM_RX_IF2GAIN_calibration_e,
    CM_RTRIM_calibration_e,
    CM_RX_LNAGAIN_calibration_e,

	CM_SMART_REFLEX_calibration_e,
	CM_CHANNEL_RESPONSE_calibration_e

}CALIBRATION_COMMANDS_ENM;


typedef enum CALIBRATIONS_ENMT
{
	FIRST_CALIBRATION_TYPE_E,
/*----------------------------------------------------------*/
	/**** GENERAL ****/
	DRPW_RFCALIBFXN_RXTXLPF_TYPE_E = FIRST_CALIBRATION_TYPE_E,
	DRPW_TUNE_TYPE_E,	/* TUNE will perform DCO_freq, AB/TB, KDCO, TDC_inverter */
	DRPW_RFCALIBFXN_RTRIM_TYPE_E,
	/**** TX ****/
	CM_TX_LO_LEAKAGE_CALIBRATION_TYPE_E,
    CM_TX_IQ_MM_CALIBRATION_TYPE_E,
	DRPW_RFCALIBFXN_TXMIXERFREQ_TYPE_E,
	/**** RX ****/
	DRPW_RFCALIBFXN_TA_TYPE_E,
	DRPW_RFCALIBFXN_RXLNAGAIN_TYPE_E,
	DRPW_RFCALIBFXN_RXIF2GAIN_TYPE_E,
	DRPW_RFCALIBFXN_RXDAC_TYPE_E,
	DRPW_RFCALIBFXN_LNATANK_TYPE_E,
	RX_ANALOG_DC_CORRECTION_CALIBRATION_TYPE_E,
	CM_RX_IQ_MM_CORRECTION_CALIBRATION_TYPE_E,
    SMART_REFLEX_CALIBRATION_TYPE_E,
    CHANNEL_RESPONSE_CALIBRATION_TYPE_E,
	/* ... */
/*----------------------------------------------------------*/
	NUMBER_OF_CALIBRATIONS_E,
	LAST_CALIBRATION_TYPE_E = (NUMBER_OF_CALIBRATIONS_E - 1)
} CALIBRATIONS_ENMT;

/******************************************************************************

    Name:	ACX_CAL_ASSESSMENT
	Type:	Configuration
	Access:	Write Only
	Length: 4
	Note:	OBSOLETE !!! (DO_CALIBRATION_IN_DRIVER is not defined)
	
******************************************************************************/
typedef enum
{
    RUNTIME_CALIBRATION_NOT_NEEDED = 1,
    RUNTIME_CALIBRATION_NEEDED = 2,
    RFPLL_CALIBRATION_NEEDED = 3,
    MAX_RUNTIME_CALIBRATION_OPTIONS = 0x7FFFFFFF /* force this enum to be uint32 */
} RadioRuntimeCalState_enum;


#ifdef HOST_COMPILE
typedef uint32 RadioRuntimeCalState_e;
#else
typedef RadioRuntimeCalState_enum RadioRuntimeCalState_e;
#endif

/************************************************************************/
/*																		*/	
/*							Commands section                            */
/*																		*/
/************************************************************************/
typedef struct PltGainGet_t
{
            uint8 TxGain;            /*Total TX chain gain according to the current setting*/
            uint8 TxUpperBound;      /*the max gain setting allowed*/
            uint8 TxLowerBound;      /*the min gain setting allowed*/
            uint8 padding;           /* padding to 32 bit */
}PltGainGet_t;

typedef struct 
{
    uint8 refTxPower;
    uint8 band;
    uint8 channel;
    uint8 padding;
}RadioPltTxCalibrationRequest_t;


/******************************************************************************

Name:	PowerLevelTable_t
Desc:   Retrieve Maximum Dbm per power level and sub-band.
Type:	Configuration
Access:	Read Only
Length: 20 

******************************************************************************/
typedef struct
{
	uint8 txPowerTable[NUMBER_OF_SUB_BANDS_E][NUM_OF_POWER_LEVEL]; /* Maximun Dbm in Dbm/10 units */
} PowerLevelTable_t;

/* DORONS [4/27/2008] testing the 2nd auxiliary function */
typedef struct
{
    int8 desiredTone;
    int8 desiredGain;
    uint8 mode;
    uint8 padding;
} TestToneParams_t;

typedef enum
{
	ePM_AWAKE,
	ePM_LISTEN_ENTER,
	ePM_LISTEN_EXIT,
	ePM_POWER_DOWN_ENTER,
	ePM_POWER_DOWN_EXIT,
	ePM_ELP_ENTER,
	ePM_ELP_EXIT,
	ePM_CORTEX_GATE_ENTER,
	ePM_CORTEX_GATE_EXIT
}PowerMode;

typedef struct 

{
	uint8	iPowerMode;		/* Awake					- 0 */
							/* Enter Listen Mode		- 1 */
							/* Exit Listen Mode			- 2 */
							/* Enter Power Down Mode	- 3 */
							/* Exit Power Down Mode		- 4 */
							/* ELP Mode					- 5 */							
							/* Enter Cortex Gate Mode	- 6 */
							/* Exit Cortex Gate Mode	- 7 */
	uint8	  padding[3];
}TTestCmdPowerMode;

/************************************************************************
                PLT  DBS
				To modify these DBs Latter - according to Architecture Document, 
				and move it to public_commands.h
************************************************************************/

/******************************************************************************

      ID:     CMD_TEST
    Desc:   The TEST command can be issued immediately after the firmware has 
          been downloaded, with no further configuration of the WiLink required.
          Full initialization of the WiLink is not required to invoke the TEST 
          command and perform the radio test function.
          After testing, the system must be reset.
          Test parameters can be modified while a test is executing. 
          For instance, the host program can change the channel without resetting
          the system.
    
      Params:     TestCmdID_enum - see below.
          The returned values are copied to the cmd/sts MB replacing  the command
          (similar to the interrogate mechanism).

******************************************************************************/
/* Efil -	when adding parameter here fill the switch case sentence in function
			"cmdBld_CmdIeTest" in module "TWD\Ctrl\CmdBldCmdIE.c" */
typedef enum
{	
/*	0x01	*/  TEST_CMD_PD_BUFFER_CAL = 0x1,	/* TX PLT */
/*	0x02	*/  TEST_CMD_P2G_CAL,				/* TX BiP */
/*	0x03	*/  TEST_CMD_RX_PLT_ENTER,
/*	0x04	*/  TEST_CMD_RX_PLT_CAL,			/* RSSI Cal */
/*	0x05	*/  TEST_CMD_RX_PLT_EXIT,
/*	0x06	*/  TEST_CMD_RX_PLT_GET,
/*	0x07	*/  TEST_CMD_FCC,					/* Continuous TX */ 
/*	0x08	*/  TEST_CMD_TELEC,					/* Carrier wave in a specific channel and band */
/*	0x09	*/  TEST_CMD_STOP_TX,				/* Stop FCC or TELEC */
/*	0x0A	*/  TEST_CMD_PLT_TEMPLATE,			/* define Template for TX */
/*	0x0B	*/  TEST_CMD_PLT_GAIN_ADJUST,
/*	0x0C	*/  TEST_CMD_PLT_GAIN_GET,
/*	0x0D	*/	TEST_CMD_CHANNEL_TUNE,
/*	0x0E	*/	TEST_CMD_FREE_RUN_RSSI,         /* Free running RSSI measurement */
/*	0x0F	*/  TEST_CMD_DEBUG,					/* test command for debug using the struct: */
/*	0x10	*/  TEST_CMD_CLPC_COMMANDS,
/*	0x11	*/  RESERVED_4,
/*	0x12	*/  TEST_CMD_RX_STAT_STOP,
/*	0x13	*/  TEST_CMD_RX_STAT_START,
/*	0x14	*/  TEST_CMD_RX_STAT_RESET,
/*	0x15	*/  TEST_CMD_RX_STAT_GET,
/*	0x16	*/	TEST_CMD_LOOPBACK_START,		/* for FW Test Debug */
/*	0x17	*/  TEST_CMD_LOOPBACK_STOP,			/* for FW Test Debug */
/*	0x18	*/	TEST_CMD_GET_FW_VERSIONS,
/*  0x19    */  TEST_CMD_INI_FILE_RADIO_PARAM,
/*  0x1A	*/  TEST_CMD_RUN_CALIBRATION_TYPE,
/*  0x1B    */	TEST_CMD_TX_GAIN_ADJUST,
/*	0x1C	*/	TEST_CMD_UPDATE_PD_BUFFER_ERRORS,
/*	0x1D	*/	TEST_CMD_UPDATE_PD_REFERENCE_POINT,
/*  0x1E    */	TEST_CMD_INI_FILE_GENERAL_PARAM,
/*	0x1F	*/	TEST_CMD_SET_EFUSE,
/*	0x20	*/	TEST_CMD_GET_EFUSE, 
/* DORONS [4/27/2008] testing the 2nd auxiliary function */
/*0x21 */   TEST_CMD_TEST_TONE,
/*	0x22	*/	TEST_CMD_POWER_MODE,
/*	0x23	*/	TEST_CMD_SMART_REFLEX,
/*	0x24	*/	TEST_CMD_CHANNEL_RESPONSE,
/*	0x25	*/	TEST_CMD_DCO_ITRIM_FEATURE,
/*	0x26	*/	TEST_CMD_INI_FILE_RF_EXTENDED_PARAM,

    MAX_TEST_CMD_ID = 0xFF	/* Dummy - must be last!!! (make sure that Enum variables are type of int) */
        
} TestCmdID_enum;

/************************************************************************/
/* radio test result information struct									*/
/************************************************************************/	
#define DEFAULT_MULTIPLE_ACTIVATION_TIME		5

#define MULTIPLE_ACTIVATION_TIME				1000000		

#define DEFAULT_RSMODE_CALIBRATION_INTERVAL		(DEFAULT_MULTIPLE_ACTIVATION_TIME * MULTIPLE_ACTIVATION_TIME)	/* RadioScope calibration interval - 5 sec */

typedef enum
{
	eCMD_GET_CALIBRAIONS_INFO,
	eCMD_GET_CLPC_VBAT_TEMPERATURE_INFO
}TTestCmdDeubug_enum;

/* struct of calibration status, indication if RM performed calibration */
typedef struct  
{
	uint8		operateCalibration;									/* RM performed calibration */
	int8		calibrationsResult[NUMBER_OF_CALIBRATIONS_E];		/* Calibrations status	*/
}CalibrationInfo;

/* struct of CLPC output, temperature, battery voltage */
typedef struct  
{
	int					ClpcOffset[NUMBER_OF_RATE_GROUPS_E];		 /* CLPC */
	int8				CurrentTemperature;							 /* current temperature in Celsius */
	uint16				CurrentVbat;								 /* VBat	*/
	
}CLPCTempratureVbatStruct;

typedef struct  
{
	int16			oRadioStatus;
	uint8			iCommand; /* command to check */
	
	CalibrationInfo				calibInfo;			/* for eCMD_GET_CALIBRAIONS_INFO */
	CLPCTempratureVbatStruct	CLPCTempVbatInfo;	/* for eCMD_GET_CLPC_VBAT_TEMPERATURE_INFO */

	uint8			padding[3];
	
}TTestCmdDebug;

/************************************************************************/
/* end radio test result information struct								*/
/************************************************************************/	


#ifdef HOST_COMPILE
typedef uint8 TestCmdID_e;
#else
typedef TestCmdID_enum TestCmdID_e;
#endif

/******************************************************************************/
typedef enum
{
    TEST_MODE_HOST_ORIGINATED_DATA      = 0x00,
    TEST_MODE_FIXED_SEQ_NUMBER          = 0x00,
    TEST_MODE_FW_ORIGINATED_DATA     	= 0x01,
    TEST_MODE_RANDOM_DATA               = 0x05,
    TEST_MODE_ZOZO_DATA                 = 0x09,
    TEST_MODE_FILLING_PATERN_MASK       = 0x0F,
    TEST_MODE_DELAY_REQUIRED            = 0x10,
    TEST_MODE_DISABLE_SRCRAMBLING_FLAG  = 0x20
}TestModeCtrlTypes_e;

#ifdef HOST_COMPILE
typedef uint8 FccTestType_e;
#else
typedef TestModeCtrlTypes_e FccTestType_e;
#endif

/******************************************************************************/
#define     TEST_SEQ_NUM_MODE_FIXED             (0)
#define     TEST_SEQ_NUM_MODE_INCREMENTED       (1)

/******************************************************************************/
/* DORONS [4/23/2008] RX Tone activation for DRPw cals */
#define     ACTIVE_TONE_CAL_MODE                (0)
#define     ACTIVE_TONE_NORM_MODE               (1)
/******************************************************************************

  TestCmdId :   TEST_CMD_FCC - Tx continuous test

  Description:  Continuous transmit series of numbers with a valid MAC header
                as was received from driver.
                However there is no 802.11 air access compliance. 

  Params:       PERTxCfg_t fcc - see below.

******************************************************************************/
#define NUM_OF_MAC_ADDR_ELEMENTS 6
typedef struct PERTxCfg_t
{
            /*input parameters*/
            uint32 numFrames;       				/* number of frams to transmit, 0 = endless*/
            uint32 interFrameGap;   				/* time gap in uSec */          
            uint32 seqNumMode;      				/* Fixed / Incremented */
            uint32 frameBodySize;    				/* length of Mac Payload */       
            uint8 channel;          				/*channel number*/
            uint8 dataRate;         				/* MBps 1,2,11,22,... 54           */
            uint8 modPreamble;      				/* CTL_PREAMBLE 0x01 */           
            uint8 band;             				/* {BAND_SELECT_24GHZ 0x00 | BAND_SELECT_5GHZ 0x01} */          
            uint8 modulation;       				/* {PBCC_MODULATION_MASK |OFDM_MODULATION_MASK }*/      
            FccTestType_e testModeCtrl;     
            uint8 dest[NUM_OF_MAC_ADDR_ELEMENTS];  	/* set to hard codded default {0,0,0xde,0xad,0xbe,0xef}; */        
} PERTxCfg_t;

/******************************************************************************

  TestCmdId :   TEST_CMD_SET_EFUSE, TEST_CMD_GET_EFUSE

  Description:  Get and set the eFuse parameters 

******************************************************************************/
typedef enum EFUSE_PARAMETER_TYPE_ENMT
{
	EFUSE_FIRST_PARAMETER_E,
/*_______________________________________________*/

	/* RX PARAMETERS */
    EFUSE_FIRST_RX_PARAMETER_E = EFUSE_FIRST_PARAMETER_E,
	RX_BIP_MAX_GAIN_ERROR_BAND_B_E = EFUSE_FIRST_RX_PARAMETER_E,		/* MaxGainErrBandB */
																		
	RX_BIP_MAX_GAIN_ERROR_J_LOW_MID_E,									/* MaxGainErrJLowMid */
	RX_BIP_MAX_GAIN_ERROR_J_HIGH_E,										/* MaxGainErrJHigh  */
																		
	RX_BIP_MAX_GAIN_ERROR_5G_1ST_E,										/* MaxGainErr5G1st  */
	RX_BIP_MAX_GAIN_ERROR_5G_2ND_E,										/* MaxGainErr5G2nd  */
	RX_BIP_MAX_GAIN_ERROR_5G_3RD_E,										/* MaxGainErr5G3rd  */
	RX_BIP_MAX_GAIN_ERROR_5G_4TH_E,										/* MaxGainErr5G4th  */
																		
	RX_BIP_LNA_STEP_CORR_BAND_B_4TO3_E,									/* LnaStepCorrBandB (Step 4To3) */
	RX_BIP_LNA_STEP_CORR_BAND_B_3TO2_E,									/* LnaStepCorrBandB (Step 3To2) */
	RX_BIP_LNA_STEP_CORR_BAND_B_2TO1_E,									/* LnaStepCorrBandB (Step 2To1) */
	RX_BIP_LNA_STEP_CORR_BAND_B_1TO0_E,									/* LnaStepCorrBandB (Step 1To0) */
																		
	RX_BIP_LNA_STEP_CORR_BAND_A_4TO3_E,									/* LnaStepCorrBandA (Step 4To3) */
	RX_BIP_LNA_STEP_CORR_BAND_A_3TO2_E,									/* LnaStepCorrBandA (Step 3To2) */
	RX_BIP_LNA_STEP_CORR_BAND_A_2TO1_E,									/* LnaStepCorrBandA (Step 2To1) */
	RX_BIP_LNA_STEP_CORR_BAND_A_1TO0_E,									/* LnaStepCorrBandA (Step 1To0) */
																		
	RX_BIP_TA_STEP_CORR_BAND_B_2TO1_E,									/* TaStepCorrBandB (Step 2To1) */
	RX_BIP_TA_STEP_CORR_BAND_B_1TO0_E,									/* TaStepCorrBandB (Step 1To0) */
																		
	RX_BIP_TA_STEP_CORR_BAND_A_2TO1_E,									/* TaStepCorrBandA (Step 2To1) */
	RX_BIP_TA_STEP_CORR_BAND_A_1TO0_E,									/* TaStepCorrBandA (Step 1To0) */
																		
	NUMBER_OF_RX_BIP_EFUSE_PARAMETERS_E,								/* Number of RX parameters */

	/* TX PARAMETERS */
	TX_BIP_PD_BUFFER_GAIN_ERROR_E = NUMBER_OF_RX_BIP_EFUSE_PARAMETERS_E,/* PD_Buffer_Gain_error */
	TX_BIP_PD_BUFFER_VBIAS_ERROR_E,										/* PD_Buffer_Vbias_error */

/*_______________________________________________*/
	EFUSE_NUMBER_OF_PARAMETERS_E,
    EFUSE_LAST_PARAMETER_E = (EFUSE_NUMBER_OF_PARAMETERS_E - 1)

}EFUSE_PARAMETER_TYPE_ENM;

typedef struct 
{
	int8	EfuseParameters[EFUSE_NUMBER_OF_PARAMETERS_E];

	int16	oRadioStatus;
    int8	padding[3];     /* Align to 32bit */

} EfuseParameters_t;

/******************************************************************************/

/******************************************************************************

  TestCmdId :       TEST_CMD_PLT_GAIN_GET

    Description: Retrieves the TX chain gain settings.         

  Params:           PltGainGet_t       gainGet - see public_radio.h


******************************************************************************/

/******************************************************************************

    TestCmdId:  TEST_CMD_PLT_GET_NVS_UPDATE_BUFFER
    
    Description: This PLT function provides the all information required by 
                    the upper driver in order to update the NVS image.
                    It received a parameter defining the type of update 
                    information required and provides an array of elements 
                    defining the data bytes to be written to the NVS image 
                    and the byte offset in which they should be written.         
 Params:     PltNvsResultsBuffer_t nvsUpdateBuffer  - see public_radio.h
    

*****************************************************************************/


/******************************************************************************

  TestCmdId :   TEST_CMD_PLT_GAIN_ADJUST

    Description: retrieves the TX chain gain settings.                        

    Params:     int32                txGainAdjust

*****************************************************************************/

/******************************************************************************

  TestCmdId :   TEST_CMD_PLT_RX_CALIBRATION

    Description: Used as part of the  RX calibration procedure, call this 
            function for every calibration channel. 
            The response for that function indicates only that command had been received by th FW,
            and not that the calibration procedure had been finished.
            The upper layer need to wait amount of ((numOfSamples*intervalBetweenSamplesUsec).
            To make sure that the RX  calibration  completed. before calling to the next command.
            
    Params:     PltRxCalibrationRequest_t    rxCalibration 
  
  ******************************************************************************/

typedef struct
{
	uint8 iBand;
	uint8 iChannel;
	int16 oRadioStatus;
} TTestCmdChannel;

typedef struct TTestCmdPdBufferCalStruct
{
	uint8   iGain;
	uint8   iVBias; 
	int16	oAdcCodeword;
	int16	oRadioStatus;
	uint8	Padding[2];
} TTestCmdPdBufferCal;

typedef struct 
{
	int8	vBIASerror;
	int8	gainError;
	uint8	padding[2];
}TTestCmdPdBufferErrors;

typedef struct 
{
	int32     iReferencePointPower;
	int32     iReferencePointDetectorValue;
	uint8     isubBand;
	uint8     padding[3];
}TTestCmdUpdateReferncePoint;

typedef struct
{
	int16 oRadioStatus;
	uint8 iCalibratonType;
	uint8 Padding;

} TTestCmdRunCalibration;

typedef struct 
{
    uint8   DCOItrimONOff;
	uint8	padding[3];
}TTestCmdDCOItrimOnOff;

typedef enum
{
	eDISABLE_LIMIT_POWER,
	eENABLE_LIMIT_POWER
}UseIniFileLimitPower;

typedef struct 
{
	int32	iTxGainValue;
	int16	oRadioStatus;
	uint8	iUseinifilelimitPower;
	uint8	padding;
} TTxGainAdjust;

/* TXPWR_CFG0__VGA_STEP_GAIN_E */
typedef enum TXPWR_CFG0__VGA_STEP_ENMT
{
	TXPWR_CFG0__VGA_STEP__FIRST_E,					
/*_______________________________________________*/
	TXPWR_CFG0__VGA_STEP__MINIMUM_E = TXPWR_CFG0__VGA_STEP__FIRST_E,
	TXPWR_CFG0__VGA_STEP__0_E = TXPWR_CFG0__VGA_STEP__MINIMUM_E,	
	TXPWR_CFG0__VGA_STEP__1_E,						
	TXPWR_CFG0__VGA_STEP__2_E,						
	TXPWR_CFG0__VGA_STEP__3_E,						
	TXPWR_CFG0__VGA_STEP__4_E,					
	TXPWR_CFG0__VGA_STEP__MAXIMUM_E = TXPWR_CFG0__VGA_STEP__4_E,
/*_______________________________________________*/
	TXPWR_CFG0__VGA_STEP__NUMBER_OF_STEPS_E,				
	TXPWR_CFG0__VGA_STEP__LAST_E = (TXPWR_CFG0__VGA_STEP__NUMBER_OF_STEPS_E - 1)

} TXPWR_CFG0__VGA_STEP_ENM;


/******************************************************************************

	Name:	ACX_PLT_NVS_BUFFER_UPDATE 
	TestCmdId:	TEST_CMD_PLT_GET_NVS_UPDATE_BUFFER
	Description: This PLT function provides the all information required by 
					the upper driver in order to update the NVS image.
					It received a parameter defining the type of update 
					information required and provides an array of elements defining 
					the data bytes to be written to the NVS image and the byte 
					offset in which they should be written.         
	Type:	PLT
	Access:	Read Only
	Length: 420

******************************************************************************/

/* default efuse value */
#define DEFAULT_EFUSE_VALUE				0

/* Default hard-coded power to gain offsets (these values will be overridden by NVS) */
#define DB_FACTOR						1000			/* factor because we can't use float */

/* TX BIP default parameters */
#define CALIBRATION_STEP_SIZE			1000
#define CALIBRATION_POWER_HIGHER_RANGE	22000
#define CALIBRATION_POWER_LOWER_RANGE	(-3000)

#define FIRST_PD_CURVE_TO_SET_2_OCTET	(10 * CALIBRATION_STEP_SIZE)/* dBm */

#define SIZE_OF_POWER_DETECTOR_TABLE	((((CALIBRATION_POWER_HIGHER_RANGE) - (CALIBRATION_POWER_LOWER_RANGE))\
	                                      / (CALIBRATION_STEP_SIZE)) + 1)

/* default PPA steps value */
#define DEFAULT_PPA_STEP_VALUE			(-6000)

#define P2G_TABLE_TO_NVS				(-1) * 8 / DB_FACTOR

#define DEF_2_4_G_SUB_BAND_P2G_OFFSET           (-25000)
#define DEF_LOW_JAPAN_4_9_G_SUB_BAND_P2G_OFFSET (-25*DB_FACTOR)
#define DEF_MID_JAPAN_4_9_G_SUB_BAND_OFFSET     (-25*DB_FACTOR)
#define DEF_HIGH_JAPAN_4_9_G_SUB_BAND_OFFSET    (-25*DB_FACTOR)
#define DEF_5_G_FIRST_SUB_BAND_P2G_OFFSET       (-25*DB_FACTOR)
#define DEF_5_G_SECOND_SUB_BAND_P2G_OFFSET      (-25*DB_FACTOR)
#define DEF_5_G_THIRD_SUB_BAND_P2G_OFFSET       (-25*DB_FACTOR)
#define DEF_5_G_FOURTH_SUB_BAND_P2G_OFFSET      (-25*DB_FACTOR)


/* NVS definition start here */

#define	NVS_TX_TYPE_INDEX		0 
#define        NVS_TX_LENGTH_INDEX                             ((NVS_TX_TYPE_INDEX) + 1) /* 1 (26) */
#define        NVS_TX_PARAM_INDEX                              ((NVS_TX_LENGTH_INDEX) + 2) /* 3  (28) */

#define START_TYPE_INDEX_IN_TLV	0
#define TLV_TYPE_LENGTH			1
#define START_LENGTH_INDEX		(START_TYPE_INDEX_IN_TLV + TLV_TYPE_LENGTH) /* 1 */
#define TLV_LENGTH_LENGTH		2
#define START_PARAM_INDEX		(START_LENGTH_INDEX + TLV_LENGTH_LENGTH) /* 3 */

#define	NVS_VERSION_1			1
#define	NVS_VERSION_2			2

#define	NVS_MAC_FIRST_LENGTH_INDEX			0
#define	NVS_MAC_FIRST_LENGHT_VALUE			1

#define NVS_MAC_L_ADDRESS_INDEX				((NVS_MAC_FIRST_LENGTH_INDEX) + 1) /* 1*/
#define NVS_MAC_L_ADDRESS_LENGTH			2

#define NVS_MAC_L_VALUE_INDEX				((NVS_MAC_L_ADDRESS_INDEX) + (NVS_MAC_L_ADDRESS_LENGTH)) /* 3 */
#define NVS_MAC_L_VALUE_LENGTH				4

#define	NVS_MAC_SECONDE_LENGTH_INDEX		((NVS_MAC_L_VALUE_INDEX) + 4) /* 7 */
#define	NVS_MAC_SECONDE_LENGHT_VALUE		1

#define NVS_MAC_H_ADDRESS_INDEX				((NVS_MAC_SECONDE_LENGTH_INDEX) + 1) /* 8*/
#define NVS_MAC_H_ADDRESS_LENGTH			2

#define NVS_MAC_H_VALUE_INDEX				((NVS_MAC_H_ADDRESS_INDEX) + (NVS_MAC_H_ADDRESS_LENGTH)) /* 10 */
#define NVS_MAC_H_VALUE_LENGTH				4

#define NVS_END_BURST_TRANSACTION_INDEX		((NVS_MAC_H_VALUE_INDEX) + (NVS_MAC_H_VALUE_LENGTH))	/* 14 */
#define NVS_END_BURST_TRANSACTION_VALUE		0
#define NVS_END_BURST_TRANSACTION_LENGTH	7

#define NVS_ALING_TLV_START_ADDRESS_INDEX	((NVS_END_BURST_TRANSACTION_INDEX) + (NVS_END_BURST_TRANSACTION_LENGTH))	/* 21 */
#define NVS_ALING_TLV_START_ADDRESS_VALUE	0
#define NVS_ALING_TLV_START_ADDRESS_LENGTH	3


/* NVS pre TLV length */
#define NVS_PRE_PARAMETERS_LENGTH			((NVS_ALING_TLV_START_ADDRESS_INDEX) + (NVS_ALING_TLV_START_ADDRESS_LENGTH)) /* 24 */

/* NVS P2G table */
#define NVS_TX_P2G_TABLE_LENGTH			((NUMBER_OF_SUB_BANDS_E) * 1 /* byte */) /* 8 */

/* NVS PPA table */
#define NVS_TX_PPA_STEPS_TABLE_LENGTH	((NUMBER_OF_SUB_BANDS_E) * \
                                         ((TXPWR_CFG0__VGA_STEP__NUMBER_OF_STEPS_E) \
                                          - 1) * 1 /* byte */) 	/* 32 */

/* NVS version 1 TX PD curve table length */
#define NVS_TX_PD_TABLE_LENGTH_NVS_V1	(1 /* byte to set size of table */ + \
                                         ((NUMBER_OF_SUB_BANDS_E) * (2 /* 1 byte offset, 1 byte low range */ + \
                                          2 /* first index in table */ + (((SIZE_OF_POWER_DETECTOR_TABLE) - 1) * 1 /* 1 byte */)))) /* 233 */

/* NVS version 2 TX PD curve table length */ 
#define NVS_TX_PD_TABLE_LENGTH_NVS_V2	((NUMBER_OF_SUB_BANDS_E) * (12 /* 12index of one byte -2 dBm - 9dBm */ +\
                                        28 /* 14 indexes of 2 byte -3dBm, 10dBm - 22 dBm */)) /* 320 */

/* NVS version 1 TX parameters Length */
#define	NVS_TX_PARAM_LENGTH_NVS_V1		((NVS_TX_P2G_TABLE_LENGTH) + (NVS_TX_PPA_STEPS_TABLE_LENGTH) +\
                                         (NVS_TX_PD_TABLE_LENGTH_NVS_V1)) /* 273 */

/* NVS version 2 TX parameters Length */
#define NVS_TX_PARAM_LENGTH_NVS_V2		((NVS_TX_P2G_TABLE_LENGTH) + (NVS_TX_PPA_STEPS_TABLE_LENGTH) +\
                                         (NVS_TX_PD_TABLE_LENGTH_NVS_V2) +\
                                         (NUMBER_OF_RADIO_CHANNEL_INDEXS_E /* for Per Channel power Gain Offset table */)) /* 409 */

/* NVS TX version */
#define NVS_TX_PARAM_LENGTH				NVS_TX_PARAM_LENGTH_NVS_V2

/* NVS RX version */
#define        NVS_RX_TYPE_INDEX                               ((NVS_TX_PARAM_INDEX) + (NVS_TX_PARAM_LENGTH)) /* 316 (341) */
#define        NVS_RX_LENGTH_INDEX                             ((NVS_RX_TYPE_INDEX) + 1) /* 317 (342) */
#define        NVS_RX_PARAM_INDEX                              ((NVS_RX_LENGTH_INDEX) + 2) /* 319 (344) */
#define	NVS_RX_PARAM_LENGTH				NUMBER_OF_RX_BIP_EFUSE_PARAMETERS_E				/* 19		 */

/* NVS version parameter length */
#define NVS_VERSION_TYPE_INDEX                 ((NVS_RX_PARAM_INDEX) + (NVS_RX_PARAM_LENGTH)) /* 338 (363) */
#define NVS_VERSION_LENGTH_INDEX               ((NVS_VERSION_TYPE_INDEX) + 1) /* 339 (364) */
#define NVS_VERSION_PARAMETER_INDEX            ((NVS_VERSION_LENGTH_INDEX) + 2) /* 340 (365) */
#define NVS_VERSION_PARAMETER_LENGTH	3

/* NVS max length */
#define NVS_TOTAL_LENGTH				500 /* original ((NVS_TOTAL_LENGTH) + 4 - ((NVS_TOTAL_LENGTH) % 4)) */

/* TLV max length */
#define  MAX_TLV_LENGTH 				NVS_TOTAL_LENGTH  
 
#define	 MAX_NVS_VERSION_LENGTH			12

/* type to set in the NVS for each mode of work */ 
typedef enum
{
	eNVS_VERSION = 0xaa,
	eNVS_RADIO_TX_PARAMETERS = 1,
	eNVS_RADIO_RX_PARAMETERS = 2,

	eNVS_RADIO_INI = 16,


	eNVS_NON_FILE = 0xFE,

	/* last TLV type */
	eTLV_LAST = 0xFF
}NVSType;

/* type to set parameter type buffers for each mode of work */
typedef enum
{
	eFIRST_RADIO_TYPE_PARAMETERS_INFO,											/* 0 */
	eNVS_RADIO_TX_TYPE_PARAMETERS_INFO = eFIRST_RADIO_TYPE_PARAMETERS_INFO,		/* 0 */
	eNVS_RADIO_RX_TYPE_PARAMETERS_INFO,											/* 1 */
	eLAST_RADIO_TYPE_PARAMETERS_INFO = eNVS_RADIO_RX_TYPE_PARAMETERS_INFO,		/* 1 */
	UNUSED_RADIO_TYPE_PARAMETERS_INFO,											/* 2 */
	eNUMBER_RADIO_TYPE_PARAMETERS_INFO = UNUSED_RADIO_TYPE_PARAMETERS_INFO,		/* 2 */
	LAST_RADIO_TYPE_PARAMETERS_INFO = (eNUMBER_RADIO_TYPE_PARAMETERS_INFO - 1)	/* 1 */
}NVSTypeInfo;

/* NVS definition end here */


typedef enum
{
	eCURRENT_SUB_BAND,
	eALL_SUB_BANDS	
}TxBipCurrentAllSubBand;

typedef struct 
{
	uint16 	Length;			       	/* TLV length in bytes */
	uint8 	Buffer[MAX_TLV_LENGTH]; /* TLV buffer content to be burned */
    uint8   Type;                   /* TLV Type Index */ 
	uint8   padding;
}TNvsStruct;

typedef struct
{
	uint32		oNVSVersion;
	TNvsStruct	oNvsStruct; 	/* output (P2G array) */
	int16		oRadioStatus;
    uint8             iSubBandMask;             /* 7 sub-band bit mask (asserted bit - calibration required) */
    uint8             Padding;
} TTestCmdP2GCal;

typedef struct
{
	int16			oRadioStatus;
	uint16			Pad;
	uint32			iDelay;			/* between packets (usec) */
	uint32	     	iRate; 			/* 1MBPS	= 0x00000001,
										2MBPS   = 0x00000002,
										5.5MBPS	= 0x00000004,
										6MBPS   = 0x00000008,
										9MBPS   = 0x00000010,
										11MBPS  = 0x00000020,
										12MBPS  = 0x00000040,
										18MBPS  = 0x00000080,
										24MBPS  = 0x00000200,
										36MBPS  = 0x00000400,
										48MBPS  = 0x00000800,
										54MBPS  = 0x00001000,
										MCS_0  	= 0x00002000,
										MCS_1  	= 0x00004000,
										MCS_2  	= 0x00008000,
										MCS_3  	= 0x00010000,
										MCS_4  	= 0x00020000,
										MCS_5  	= 0x00040000,
										MCS_6  	= 0x00080000,
										MCS_7  	= 0x00100000 */
	uint16	     	iSize; 			/* size of packet (bytes) */
	uint16			iAmount; 		/* in case of multiple (# of packets) */
	int32			iPower;			/* upper power limit (dBm) */
	uint16			iSeed;	
	uint8			iPacketMode; 	/* single, multiple, InfiniteLength, Continuous, FCC */
	uint8	     	iDcfOnOff; 		/* use DCF access (1) */
	uint8	     	iGI;			/* Guard Interval: long:800ns (0), short:400ns (1) */
	uint8	     	iPreamble;		/* long (0), short (1),  OFDM (4), GF (7), Mixed (6) */
	uint8	     	iType;			/* Data (0), Ack (1), Probe-request(2), Random (3), User-defined (4), PER (5) */
	uint8	     	iScrambler;		/* Off (0), On (1) */
	uint8	     	iEnableCLPC; 	/* range 0-100. 0 - disable calibration										/
									   range 1-99 - enable Cal asses periodic time, every step is 200msecond 
	                                   periodic of cal assess for example: 1.2 second put the value 6.
									   if the value is out of range it will be change to 25 represent 
	                                   5 second of cal assess periodical */	
	uint8 	     	iSeqNumMode; 	/* Fixed sequence number (0), incremental (1) - used for PER test only */
	TMacAddr	 	iSrcMacAddr; 	/* Source address (BSSID) - used for PER test only */
	TMacAddr	    iDstMacAddr; 	/* Destination address - used for PER test only */
	
} TPacketParam;

typedef struct
{
	int16			 oRadioStatus;
	uint16			 Pad;
	int32		     iPower;
	uint8	    	 iToneType;
	uint8		     iPpaStep;
	uint8		     iToneNumberSingleTones;
	uint8 	    	 iToneNumberTwoTones;
	uint8		     iUseDigitalDC;
	uint8 		     iInvert;
	uint8	    	 iElevenNSpan;
	uint8		     iDigitalDC;
	uint8		     iAnalogDCFine;
	uint8	    	 iAnalogDCCoarse;
} TToneParam;

typedef  struct 
{
	uint16 	bufferOffset;
	uint16 	bufferLength;
	int16	oRadioStatus;
	int8 	buffer[TX_TEMPLATE_MAX_BUF_LEN];
	uint8	padding[2];
} TTxTemplate;

typedef enum
{
	eDISABLE_CLPC,
	eENABLE_CLPC,
	eRESET_CLPC_TABLES,
	eINIDCATE_CLPC_ACTIVATION_TIME
}CLPCCommands;

typedef struct 
{
	int16	oStatus;
	uint8	iCLPCActivationTime; /* range 0-100. 0 - disable calibration										/
									range 1-99 - enable Cal asses periodic time, every step is 200msecond 
	                                periodic of cal assess for example: 1.2 second put the value 6.
									if the value is out of range it will be change to 25 represent 
	                                5 second of cal assess periodical */
	uint8	iCLPCCommands;
}TTestCmdCLPCCommands;


/************************************************************************
                PLT  DBS
				Theses DBs were moved from the TWDExternalIf.h because of
				redundency.
************************************************************************/
typedef struct 
{
	uint8   oAbsoluteGain; 	/* Per Sub-Band (output) */
	uint8   oLNASteps[RX_PLT_LNA_STEPS_BUF_LEN]; 	/* 4 steps per Band (output) */
	uint8   oTASteps[RX_PLT_TA_STEPS_BUF_LEN]; 	/* 2 steps per Band (output) */
	uint8   Padding; 
} TTestCmdRxPlt;

typedef struct 
{
	uint32  ReceivedValidPacketsNumber;
    uint32  ReceivedFcsErrorPacketsNumber;
    uint32  ReceivedPlcpErrorPacketsNumber;
    uint32 	SeqNumMissCount; /* For PER calculation */
    int16   AverageSnr;
    int16   AverageRssi;
    int16  AverageEvm;
    uint8   Padding[2];
} RxPathStatistics_t;

typedef struct 
{
	uint16  Length; 
    uint16  EVM; 
    uint16  RSSI;
    uint16  FrequencyDelta; 
    uint16  Flags;
    int8	Type;
    uint8   Rate;
    uint8   Noise;
    uint8   AgcGain;
    uint8   Padding[2];
} RxPacketStatistics_t;

#define RX_STAT_PACKETS_PER_MESSAGE           (20) 
typedef struct 
{
	RxPathStatistics_t		oRxPathStatistics;
    uint32           		oBasePacketId; 
    uint32           		ioNumberOfPackets; 			/* input/output: number of following packets */
	uint32					oNumberOfMissedPackets;		/* number of following packet statistic entries that were dropped */
    /*RxPacketStatistics_t    RxPacketStatistics[RX_STAT_PACKETS_PER_MESSAGE];*/
	int16					oRadioStatus;
} RadioRxStatistics;

/* RX RF gain values */
typedef enum PHY_RADIO_RX_GAIN_VALUES_ENMT
{
	FIRST_RX_GAIN_VALUE_E,					
/*_______________________________________________*/
	RX_GAIN_VALUE_0_E = FIRST_RX_GAIN_VALUE_E,	
	RX_GAIN_VALUE_1_E,						
	RX_GAIN_VALUE_2_E,						
	RX_GAIN_VALUE_3_E,						
	RX_GAIN_VALUE_4_E,					
	RX_GAIN_VALUE_5_E,					
	RX_GAIN_VALUE_6_E,					
	RX_GAIN_VALUE_7_E,					
/*_______________________________________________*/
	NUMBER_OF_RX_GAIN_VALUES_E,				
	LAST_RX_GAIN_VALUE_E = (NUMBER_OF_RX_GAIN_VALUES_E - 1)

}PHY_RADIO_RX_GAIN_VALUES_ENM;

/* RX BIP */
typedef struct 
{
	uint32		oNVSVersion;
	int32		iExternalSignalPowerLevel;
	int32		oLnaTaCompensationValues[NUMBER_OF_RX_GAIN_VALUES_E-1];
	TNvsStruct	oNvsStruct; 
	int16		oRadioStatus;
	int8		padding[2];
}RadioRxPltCal;


typedef enum
{
	eSINGLE_BAND_INI_FILE,
	eDUAL_BAND_INI_FILE
}IniFileSingleDualBand;

#define SMART_REFLEX_LENGTH_INDEX				0
#define SMART_REFLEX_UPPER_LIMIT_INDEX			1	
#define SMART_REFLEX_START_ERROR_VALUE_INDEX	2

#define MAX_SMART_REFLEX_FUB_VALUES		14
/* 1. first index is the number of param	*/
/* 2. second is the higher value			*/
/* 3. 14 parameter of the correction		*/
#define MAX_SMART_REFLEX_PARAM					(MAX_SMART_REFLEX_FUB_VALUES + SMART_REFLEX_START_ERROR_VALUE_INDEX)



typedef struct 
{
	uint8	RefClk;                                 
	uint8	SettlingTime;                                                                 
	uint8	ClockValidOnWakeup;                      
	uint8	DC2DCMode;                               
	uint8	Single_Dual_Band_Solution;  
	uint8	TXBiPFEMAutoDetect;
	uint8	TXBiPFEMManufacturer;  
/*	GeneralSettingsByte	Settings; */
    uint8   GeneralSettings;


    /* smart reflex state*/
    uint8 SRState; 
    /* FUB parameters */
    int8	SRF1[MAX_SMART_REFLEX_PARAM];
    int8	SRF2[MAX_SMART_REFLEX_PARAM];
    int8	SRF3[MAX_SMART_REFLEX_PARAM];


    /* FUB debug parameters */
    int8	SR_Debug_Table[MAX_SMART_REFLEX_PARAM];
    uint8	SR_SEN_N_P;
    uint8	SR_SEN_N_P_Gain;
    uint8	SR_SEN_NRN;
    uint8	SR_SEN_PRN;
    uint8	padding[3];

}IniFileGeneralParam;  

typedef enum 
{		
	FEM_MANUAL_DETECT_MODE_E,
	FEM_AUTO_DETECT_MODE_E

}FEM_DETECT_MODE_ENM;

typedef enum 
{		
	FEM_RFMD_TYPE_E,
	FEM_TRIQUINT_TYPE_E,
	NUMBER_OF_FEM_TYPES_E

}FEM_TYPE_ENM;

typedef enum 
{		
	eREF_CLK_19_2_E,
	eREF_CLK_26_E,
	eREF_CLK_38_4_E,
	eREF_CLK_52_E

}REF_CLK_ENM;

typedef enum 
{		
	REF_CLK_NOT_VALID_E,
	REF_CLK_VALID_AND_STABLE_E

}CLK_VALID_ON_WAKEUP_ENM;

typedef enum 
{		
	BT_SPI_IS_NOT_USED_E,
	MUX_DC2DC_TO_BT_FUNC2_E

}DC2DC_MODE_ENM;

typedef enum 
{		
	SINGLE_BAND_SOLUTION_E,
	DUAL_BAND_SOLUTION_E

}SINGLE_DUAL_BAND_SOLUTION_ENM;

/* General settings byte */
typedef enum 
{		
	TELEC_CHAN_14_OFF_E,
	TELEC_CHAN_14_ON_E

}TELEC_CHAN_14_ENM;

typedef enum 
{		
	NBI_OFF_E,
	NBI_ON_E

}NBI_ENM;

#define RSSI_AND_PROCESS_COMPENSATION_TABLE_SIZE   (15)

typedef struct 
{
	/* SECTION 1: 2.4G parameters */
	uint8 RxTraceInsertionLoss_2_4G;																							
	uint8 TXTraceLoss_2_4G;																							
	int8  RxRssiAndProcessCompensation_2_4G[RSSI_AND_PROCESS_COMPENSATION_TABLE_SIZE];						
	
	/* SECTION 2: 5G parameters */										 
	uint8 RxTraceInsertionLoss_5G[NUMBER_OF_SUB_BANDS_IN_5G_BAND_E];																
	uint8 TXTraceLoss_5G[NUMBER_OF_SUB_BANDS_IN_5G_BAND_E];																
	int8  RxRssiAndProcessCompensation_5G[RSSI_AND_PROCESS_COMPENSATION_TABLE_SIZE];									

}TStatRadioParams;  

typedef struct 
{  
	/* SECTION 1: 2.4G parameters */
	uint16  TXBiPReferencePDvoltage_2_4G;								
	uint8   TxBiPReferencePower_2_4G;		
	int8  	TxBiPOffsetdB_2_4G;
	int8  	TxPerRatePowerLimits_2_4G_Normal[NUMBER_OF_RATE_GROUPS_E];							
	int8  	TxPerRatePowerLimits_2_4G_Degraded[NUMBER_OF_RATE_GROUPS_E];							
	int8    TxPerRatePowerLimits_2_4G_Extreme[NUMBER_OF_RATE_GROUPS_E];
	int8  	TxPerChannelPowerLimits_2_4G_11b[NUMBER_OF_2_4_G_CHANNELS];	
	int8    TxPerChannelPowerLimits_2_4G_OFDM[NUMBER_OF_2_4_G_CHANNELS];	
	int8    TxPDVsRateOffsets_2_4G[NUMBER_OF_RATE_GROUPS_E];												
	uint8   TxIbiasTable_2_4G[NUMBER_OF_RATE_GROUPS_E];														
	uint8   RxFemInsertionLoss_2_4G;
        uint8 	DegradedLowToNormalThr_2_4G;
        uint8 	NormalToDegradedHighThr_2_4G;

	/* SECTION 2: 5G parameters */
	uint16 	TXBiPReferencePDvoltage_5G[NUMBER_OF_SUB_BANDS_IN_5G_BAND_E];
	uint8  	TxBiPReferencePower_5G[NUMBER_OF_SUB_BANDS_IN_5G_BAND_E];			
	int8   	TxBiPOffsetdB_5G[NUMBER_OF_SUB_BANDS_IN_5G_BAND_E];								
	int8   	TxPerRatePowerLimits_5G_Normal[NUMBER_OF_RATE_GROUPS_E];							
	int8   	TxPerRatePowerLimits_5G_Degraded[NUMBER_OF_RATE_GROUPS_E];
    int8   	TxPerRatePowerLimits_5G_Extreme[NUMBER_OF_RATE_GROUPS_E];
	int8   	TxPerChannelPowerLimits_5G_OFDM[NUMBER_OF_5G_CHANNELS];                         
	int8   	TxPDVsRateOffsets_5G[NUMBER_OF_RATE_GROUPS_E];										
	int8   	TxIbiasTable_5G[NUMBER_OF_RATE_GROUPS_E];											
	uint8  	RxFemInsertionLoss_5G[NUMBER_OF_SUB_BANDS_IN_5G_BAND_E];	
    uint8  	DegradedLowToNormalThr_5G;			
    uint8   NormalToDegradedHighThr_5G;
 
}TDynRadioParams;  

typedef struct 
{
	TStatRadioParams	tStatRadioParams;
	TDynRadioParams		tDynRadioParams;
    uint8                   Padding[2];

}IniFileRadioParam;  

typedef struct 
{
	int8  TxPerChannelPowerCompensation_2_4G[HALF_NUMBER_OF_2_4_G_CHANNELS]; /* 7 */	
	int8  TxPerChannelPowerCompensation_5G_OFDM[HALF_NUMBER_OF_5G_CHANNELS]; /* 18 */
	uint8 Padding[3];

}IniFileExtendedRadioParam;  
	
/*\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

/* Describes a reference design supported by the HDK Module */
typedef struct HDKReferenceDesign_t
{
    uint16  referenceDesignId;          /* Reference design Id supported */
    uint8   nvsMajorVersion;            /* First EEPROM version supported */
    uint8   nvsMinorVersion;
    uint8   nvsMinorMinorVersion;
} THDKReferenceDesign;

typedef struct HDKModuleVersion_t
{
    uint8               ProductName;				/* '6' for WiLink6, '4' for WiLink4 */
    uint8               PgNumber;                   /* Hardware tag */
    uint8               SoftwareVersionLevel;       /* SW level number (Major SW change) */
    uint8               SoftwareVersionDelivery;    /* Delivery number inside any (Inside any level) */

    uint8					radioModuleType;                    /* The radio that is currently supported by the HDK module */
    uint8					numberOfReferenceDesignsSupported;  /* The number of reference designs supported by the HDK module */
    THDKReferenceDesign*   referenceDesignsSupported;			/* Array of reference_design supported */
	
} THDKModuleVersion;

#define FW_VERSION_LENGTH 5

typedef struct 
{	
	THDKModuleVersion	hdkVersion;
	uint8				FWVersion[FW_VERSION_LENGTH];		
    uint32               drpwVersion;
	int16				oRadioStatus;
	uint8				padding[3];		
}TFWVerisons;

typedef struct
{
    int16       RSSIVal;      /* free running RSSI value, 1dB resolution */
    int16		oRadioStatus;   
}TTestCmdFreeRSSI;

typedef struct
{
    TestCmdID_e     testCmdId;
	int8            padding[3];

	/* Efil -	when adding parameter here fill the switch case sentence in function
			"cmdBld_CmdIeTest" in module "TWD\Ctrl\CmdBldCmdIE.c" */
    union
    {
		TTestCmdChannel 				Channel;
		RadioRxPltCal 					RxPlt;          
		TTestCmdPdBufferCal 			PdBufferCal;
		TTestCmdP2GCal 					P2GCal;    
		TTestCmdPdBufferErrors			PdBufferErrors;
		TTestCmdUpdateReferncePoint		PdBufferCalReferencePoint;
		TPacketParam 					TxPacketParams;
		TToneParam 						TxToneParams;
		TTxTemplate						TxTemplateParams;
		/*uint32               			txGainAdjust; */
		TTxGainAdjust					txGainAdjust;
		RadioRxStatistics				Statistics;
		TFWVerisons						fwVersions;
		TTestCmdRunCalibration			RunCalibration;
        IniFileRadioParam				IniFileRadioParams;
		IniFileExtendedRadioParam		IniFileExtendedRadioParams;
		IniFileGeneralParam				IniFileGeneralParams;
		EfuseParameters_t				EfuseParams;
		TestToneParams_t				TestToneParams;
		TTestCmdPowerMode				powerMode;
        TTestCmdFreeRSSI                freeRSSI;
		TTestCmdCLPCCommands			clpcCommands;
		TTestCmdDCOItrimOnOff           DCOitrimFeatureOnOff;

		TTestCmdDebug					testDebug;
    }testCmd_u;
}TTestCmd;


#ifndef HOST_IF_ENUMS_DISABLED
typedef enum RadioParamType_e
{
    RADIO_PARAM_POWER_TABLE = 1,
    RADIO_PARAM_POWER_LIMIT_TABLE,
    RADIO_PARAM_POWER_ADJ_TABLE,
    RADIO_PARAM_POWER_ENABLES,
    RADIO_PABIAS_TABLE,
    RADIO_PARAM_POWER_LEVELS,

    MAX_RADIO_PARAM_TYPE = 0x7FFFFFFF /* force this enum to be uint32 */
    
} RadioParamType_e;
#else
typedef uint32 RadioParamType_e;
#endif

typedef struct RadioParam_t
{
    RadioParamType_e parameterType;
    int8  parameter[MAX_RADIO_PARAM_LEN];
} RadioParam_t;

typedef enum RadioState_e
{
    RADIO_STATE_INIT = 1,           /* Completed radio initialization */
    RADIO_STATE_TUNE = 2,           /* Completed channel tuning */
    RADIO_STATE_DC_CAL = 3,         /* Completed radio DC calibration */
    RADIO_STATE_AFE_DC_CAL =4,      /* Completed AFE DC calibration */
    RADIO_STATE_TX_MM = 5,          /* Completed transmit IQ mismatch calibration */
    RADIO_STATE_TX_EQUAL = 6,       /* Completed transmit equalization  calibration */
    RADIO_STATE_CARR_SUPP = 7,      /* Completed carrier suppression calibration */
    RADIO_STATE_TX_PWR_CTRL = 8     /* Completed transmit power control calibration (only for bg and abg radios) */

} RadioState_e;

typedef enum
{
    PS_MODE_ENTER_ELP = 0x0,
    PS_MODE_ENTER_PD = 0x1,
    PS_MODE_EXIT_FROM_ELP = 0x2,
    PS_MODE_EXIT_FROM_PD = 0x4,
    PS_MODE_ENTER_ELP_SG_EN = 0x10,
    PS_MODE_ENTER_PD_SG_EN = 0x11,
    PS_MODE_EXIT_FROM_ELP_SG_EN = 0x12,
    PS_MODE_EXIT_FROM_PD_SG_EN = 0x14,
    PS_MODE_INVALID = 0xFF

}PowerSaveMode_e;

typedef struct RadioTune_t
{
    Channel_e   channel;
    RadioBand_e band;
} RadioTune_t;

typedef struct RadioRSSIAndSNR_t
{
    int16   rssi;
    int16   snr;
}RadioRSSIAndSNR_t;

/* VBIAS values (in mili-volts) */
typedef enum PHY_RADIO_VBIAS_MV_ENMT
{
	FIRST_VBIAS_VALUE_E = -1,

	VBIAS_0MV_E = FIRST_VBIAS_VALUE_E,
	VBIAS_100MV_E = 0,
	VBIAS_200MV_E = 1,
	VBIAS_300MV_E = 2,
	VBIAS_400MV_E = 3,
	VBIAS_500MV_E = 4,
	VBIAS_600MV_E = 5,
	VBIAS_700MV_E = 6,
	VBIAS_800MV_E = 7,

	NUMBER_OF_VBIAS_VALUES_E = 9,
    LAST_VBIAS_VALUE_E = (NUMBER_OF_VBIAS_VALUES_E - 1)

}PHY_RADIO_VBIAS_MV_ENM;

/* Gain monitor values */
typedef enum PHY_RADIO_GAIN_MONITOR_TYPES_ENMT
{
	FIRST_GAIN_MONITOR_TYPE_E,
	GAIN_MONITOR_DISABLE = 0,
	GAIN_MONITOR_RESERVED = 1,
/*_______________________________________________*/
	GAIN_MONITOR_X0_5_E = 2,
	GAIN_MONITOR_X1_E = 3,
	GAIN_MONITOR_X2_E = 4,
	GAIN_MONITOR_X4_E = 5,
	GAIN_MONITOR_X8_E = 6,
	GAIN_MONITOR_X16_E = 7,
/*_______________________________________________*/
	NUMBER_OF_GAIN_MONITOR_TYPES_E = GAIN_MONITOR_X16_E,				
	LAST_GAIN_MONITOR_TYPE_E = (NUMBER_OF_GAIN_MONITOR_TYPES_E - 1)	

}PHY_RADIO_GAIN_MONITOR_TYPES_ENM;


/* TX Packet Mode; */
typedef enum
{
	eTX_MODE_SINGLE_PACKET,              /* 0 */
	eTX_MODE_MULTIPLE_PACKET,            /* 1 */
	eTX_MODE_INFINITE_LENGTH_PACKET,     /* 2 */
	eTX_MODE_CONTINUES_PACKET,           /* 3 */
	eTX_MODE_FCC_PACKET,                 /* 4 */
	eTX_MODE_SENARIO_PACKET,	     /* 5 */

	eMAX_PACKET_MODE_PACKET
}PacketTypeMode;

/* TX tone mode */
typedef enum
{
	eSILENCE_TONE_MODE,
	eCARRIER_FEED_THROUGH_MODE,
	eSINGLE_TONE_MODE,
	eTWO_TONE_MODE,
	eMULTI_TONE_MODE,

	eMax_TONE_MODE
}ToneTypeMode;


/**********************************************************************/
/*		For RSSI Calculation - Save Parameters						  */
/**********************************************************************/

typedef struct 
{
	uint16 linerEvmVal;
	uint16 ccaEcalcMonReg;
	uint16 ccaEcalcRssi;
	uint16 linerEvmPilVal;
	uint8 lanTableIndex;
	uint8 taTableIndex;
	uint8 lnaTableIndex;
	RADIO_SUB_BAND_TYPE_ENM currSubBand;
	RADIO_BAND_TYPE_ENM		currBand;
}rssiParamSave_t;



#endif	/* #ifndef PUBLIC_RADIO */
