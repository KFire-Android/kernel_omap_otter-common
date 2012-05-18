/*
 * cyttsp4_params.h
 *
 * Copyright (C) 2012 Texas Instruments
 * Author: Peter Nordström <nordstrom@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CYTTSP4_PARAMS_H_
#define _CYTTSP4_PARAMS_H_

#if defined(CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
#define CY_MAXX 800 /* //1368 //1296 //480 */
#define CY_MAXY 1280 /* //782 //828 //256 */
#else
#define CY_MAXX 1296 /* //1944//1368 //1296 //480 */
#define CY_MAXY 805 /* //1219//782 //828 //256 */
#endif

/* Touchscreen Parameters */
static const uint8_t cyttsp4_param_regs[] = {
	/*	H, L		  Enum	Name	*/
	0x00,  /* 00	DEBUGDATA0 */
	0x00,  /* 01	DEBUGDATA1 */
	0x17,  /* 02	NUM_ROWS */
	0x24,  /* 03	NUM_COLS */
	0x05, 0x10,  /* 04	TOUCH_RES_X */
	0x03, 0x25,  /* 05	TOUCH_RES_Y */
	0x4B, 0x64,  /* 06	SCREEN_SIZE_X */
	0x2F, 0x44,  /* 07	SCREEN_SIZE_Y */
	0x00, 0x04,  /* 08	CENTROID_THRESHOLD */
	0x00, 0x11,  /* 09	THRESHOLD_NOISE */
	0x01, 0x2C,  /* 0A	THRESHOLD_NEG_NOISE */
	0x00,  /* 0B	Reserved */
	0x00, 0x00,  /* 0C	Reserved */
	0x00, 0x12,  /* 0D	THRESHOLD_FINGER */
	0x00, 0xF2,  /* 0E	VEL_FINGER_X */
	0x00, 0xC8,  /* 0F	VEL_FINGER_Y */
	0x02,  /* 10	FINGER_DEBOUNCE */
	0x00, 0x05,  /* 11	NUM_TX_CLOCKS */
	0x00,  /* 12	IDAC_RANGE */
	0x52,  /* 13	IDAC_VALUE */
	0x82,  /* 14	TX_DIV_VALUE_0 */
	0x82,  /* 15	TX_DIV_VALUE_1 */
	0x82,  /* 16	TX_DIV_VALUE_2 */
	0x00, 0x00,  /* 17	Reserved */
	0x0F,  /* 18	LARGE_OBJ_THRES */
	0x07,  /* 19	FAT_FINGER_THRES */
	0x00,  /* 1A	Reserved */
	0x00,  /* 1B	Reserved */
	0x64,  /* 1C	LOW_POWER_INTV */
	0x0A,  /* 1D	REF_INTV */
	0x07, 0xD0,  /* 1E	ACTIVE_MODE_TIMEOUT */
	0x02,  /* 1F	WAKE_UP_SRC */
	0x00, 0x64,  /* 20	HOST_ALERT_PULSE_WIDTH */
	0x00,  /* 21	Reserved */
	0x02,  /* 22	XYFILTERMOVETHOLD */
	0x14,  /* 23	XYFILTERFASTTHOLD */
	0x05,  /* 24	XYFILTERSLOWCOEFF */
	0x0F,  /* 25	XYFILTERFASTCOEFF */
	0x23,  /* 26	FATFINGERLIMIT */
	0x0A,  /* 27	IDACHOLDOFF */
	0x00,  /* 28	FreqHoppingOperationalMode */
	0x00, 0x78,  /* 29	FreqHoppingNoiseThreshold */
	0x00, 0x32,  /* 2A	FreqHoppingBaseLevelDegradation */
	0x00, 0x32,  /* 2B	FreqHoppingRefreshCounterThreshold */
	0x00,  /* 2C	FreqHoppingRegretThreshold */
	0x00,  /* 2D	FreqHoppingTimeIncreaseMultiplier */
	0x03, 0xE8,  /* 2E	BaselineUpdateRate */
	0x01,  /* 2F	AdvancedFingerSeparation */
	0x0A,  /* 30	InnerEdgeGain */
	0x03,  /* 31	OuterEdgeGain */
	0x4B, 0x64,  /* 32	xDisplaySize */
	0x2F, 0x44,  /* 33	yDisplaySize */
	0x01,  /* 34	xFlipFlag */
	0x00,  /* 35	yFlipFlag */
	0x00, 0x64,  /* 36	xOffset */
	0x00, 0x64,  /* 37	yOffset */
	0x01, 0x2C,  /* 38	minFFZ9 */
	0x01, 0xF4,  /* 39	maxMFZ9 */
	0x18,  /* 3A	jitterFilterSigThold */
	0x01, 0x2C,  /* 3B	ACC_FINGER_X */
	0x01, 0x2C,  /* 3C	ACC_FINGER_Y */
};

/* Touchscreen Parameters Field Sizes (Writable: 0:Readonly; 1:Writable */
static const uint8_t cyttsp4_param_size[] = {
/*   Size	  Enum	Name				*/
	1, /* 00	DEBUGDATA0 */
	1, /* 01	DEBUGDATA1 */
	1, /* 02	NUM_ROWS */
	1, /* 03	NUM_COLS */
	2, /* 04	TOUCH_RES_X */
	2, /* 05	TOUCH_RES_Y */
	2, /* 06	SCREEN_SIZE_X */
	2, /* 07	SCREEN_SIZE_Y */
	2, /* 08	CENTROID_THRESHOLD */
	2, /* 09	THRESHOLD_NOISE */
	2, /* 0A	THRESHOLD_NEG_NOISE */
	1, /* 0B	Reserved */
	2, /* 0C	Reserved */
	2, /* 0D	THRESHOLD_FINGER */
	2, /* 0E	VEL_FINGER_X */
	2, /* 0F	VEL_FINGER_Y */
	1, /* 10	FINGER_DEBOUNCE */
	2, /* 11	NUM_TX_CLOCKS */
	1, /* 12	IDAC_RANGE */
	1, /* 13	IDAC_VALUE */
	1, /* 14	TX_DIV_VALUE_0 */
	1, /* 15	TX_DIV_VALUE_1 */
	1, /* 16	TX_DIV_VALUE_2 */
	2, /* 17	Reserved */
	1, /* 18	LARGE_OBJ_THRES */
	1, /* 19	FAT_FINGER_THRES */
	1, /* 1A	Reserved */
	1, /* 1B	Reserved */
	1, /* 1C	LOW_POWER_INTV */
	1, /* 1D	REF_INTV */
	2, /* 1E	ACTIVE_MODE_TIMEOUT */
	1, /* 1F	WAKE_UP_SRC */
	2, /* 20	HOST_ALERT_PULSE_WIDTH */
	1, /* 21	Reserved */
	1, /* 22	XYFILTERMOVETHOLD */
	1, /* 23	XYFILTERFASTTHOLD */
	1, /* 24	XYFILTERSLOWCOEFF */
	1, /* 25	XYFILTERFASTCOEFF */
	1, /* 26	FATFINGERLIMIT */
	1, /* 27	IDACHOLDOFF */
	1, /* 28	FreqHoppingOperationalMode */
	2, /* 29	FreqHoppingNoiseThreshold */
	2, /* 2A	FreqHoppingBaseLevelDegradation */
	2, /* 2B	FreqHoppingRefreshCounterThreshold */
	1, /* 2C	FreqHoppingRegretThreshold */
	1, /* 2D	FreqHoppingTimeIncreaseMultiplier */
	2, /* 2E	BaselineUpdateRate */
	1, /* 2F	AdvancedFingerSeparation */
	1, /* 30	InnerEdgeGain */
	1, /* 31	OuterEdgeGain */
	2, /* 32	xDisplaySize */
	2, /* 33	yDisplaySize */
	1, /* 34	xFlipFlag */
	1, /* 35	yFlipFlag */
	2, /* 36	xOffset */
	2, /* 37	yOffset */
	2, /* 38	minFFZ9 */
	2, /* 39	maxMFZ9 */
	1, /* 3A	jitterFilterSigThold */
	2, /* 3B	ACC_FINGER_X */
	2, /* 3C	ACC_FINGER_Y */
};


#endif /* _CYTTSP4_PARAMS_H_ */
