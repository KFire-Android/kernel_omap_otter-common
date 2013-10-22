/*****************************************************************************
 *****************************************************************************
 *  FILENAME: cyttsp4_params.h
 *  TrueTouch Host Emulator Version Information: N/A
 *  TrueTouch Firmware Version Information: 1.1 388697
 *
 *  DESCRIPTION: This file contains configuration values.
 *-----------------------------------------------------------------------------
 *  Copyright (c) Cypress Semiconductor 2012. All Rights Reserved.
 *****************************************************************************
 *****************************************************************************
 *-----------------------------------------------------------------------------
 */
#ifndef _CYTTSP4_PARAMS_H_
#define _CYTTSP4_PARAMS_H_

/*
 * use the following define for 90 degree rotation
*/
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM_FTM
#define CY_90_DEG_ROTATION
#endif

#if defined(CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_TATE)
#define CY_MAXX 800
#define CY_MAXY 1280
#else
#define CY_MAXX 1296
#define CY_MAXY 805
#endif

/* Touchscreen Parameters Amazon Jem Rev7*/
uint8_t cyttsp4_param_regs[] = {
/*	H, L		  Enum	Name				*/
	0x00,       /* 00	CHARGER/HDMI BIT (NV) */
	0x23,       /* 01	EDGE AREA (DEBOUNCE DISABLED), pixels */
	0x17,       /* 02	NUM_ROWS */
	0x24,       /* 03	NUM_COLS */
#ifndef CY_90_DEG_ROTATION
	0x05, 0x10, /* 04	TOUCH_RES_X */
	0x03, 0x25, /* 05	TOUCH_RES_Y */
#else
	0x03, 0x25, /* 04	TOUCH_RES_X */
	0x05, 0x10, /* 05	TOUCH_RES_Y */
#endif
	0x4C, 0x10, /* 06	SCREEN_SIZE_X */
	0x30, 0x02, /* 07	SCREEN_SIZE_Y */
	0x00, 0x05, /* 08	CENTROID_THRESHOLD */
	0x00, 0x11, /* 09	THRESHOLD_NOISE */
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM_FTM
	0x00, 0x46, /* 0A	THRESHOLD_NEG_NOISE */
#else
	0xFF, 0x00, /* 0A  THRESHOLD_NEG_NOISE */
#endif
	0x00,       /* 0B	RESERVED03 */
	0x00, 0x00, /* 0C	RESERVED04 */
	0x00, 0x12, /* 0D	THRESHOLD_FINGER */
	0x00, 0x19, /* 0E	RX_LINE_FILTER_THRESH (No charger) */
	0x00, 0x02, /* 0F	FINGER DEBOUNC (With Charger) */
	0x01,       /* 10	FINGER_DEBOUNCE */
	0x00, 0x07, /* 11	NUM_TX_CLOCKS */
	0x00,       /* 12	IDAC_RANGE */
	0x41,       /* 13	IDAC_VALUE */
	0x57,       /* 14	TX_DIV_VALUE_0 */
	0x6D,       /* 15	TX_DIV_VALUE_1 */
	0x5F,       /* 16	TX_DIV_VALUE_2 */
	0x00, 0x00, /* 17	RESERVED05 */
	0x10,       /* 18	LARGE_OBJ_THRES */
	0x09,       /* 19	FAT_FINGER_THRES */
	0x00,       /* 1A	RESERVED06 */
	0x00,       /* 1B	RESERVED07 */
	0x19,       /* 1C	LOW_POWER_INTV */
	0x0A,       /* 1D	REF_INTV */
	0x07, 0xD0, /* 1E	ACTIVE_MODE_TIMEOUT */
	0x02,       /* 1F	WAKE_UP_SRC */
	0x00, 0x64, /* 20	HOST_ALERT_PULSE_WIDTH */
	0x00,       /* 21	RESERVED08 */
	0x02,       /* 22	XYFILTERMOVETHOLD */
	0x05,       /* 23	XYFILTERFASTTHOLD */
	0x05,       /* 24	XYFILTERSLOWCOEFF */
	0x0F,       /* 25	XYFILTERFASTCOEFF */
	0x32,       /* 26	RX_LINE_FILTER_THRESH (With charger) */
	0x05,       /* 27	IDACHOLDOFF */
	0x00,       /* 28	FreqHoppingOperationalMode */
	0x00, 0x5A, /* 29	FreqHoppingNoiseThreshold */
	0x00, 0x02, /* 2A	FreqHoppingBaseLevelDegradation */
	0x00, 0x1E, /* 2B	FreqHoppingRefreshCounterThreshold */
	0x0E,       /* 2C	Dynamic Finger Threshold Coefficient */
	0x64,       /* 2D	Maximum Dynamic Finger Threshold */
#ifndef CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM_FTM
	0x00, 0x14, /* 2E	BaselineUpdateRate */
#else
	0x02, 0x58, /* 2E       BaselineUpdateRate */
#endif
	0x01,       /* 2F	AdvancedFingerSeparation */
	0x0A,       /* 30	InnerEdgeGain */
	0x03,       /* 31	OuterEdgeGain */
	0x4A, 0xD0, /* 32	XDISPLAYSIZE */
	0x2E, 0xC2, /* 33	YDISPLAYSIZE */
	0x01,       /* 34	XFLIP */
	0x00,       /* 35	YFLIP */
	0x00, 0x6E, /* 36	XOFFSET */
	0x00, 0x5A, /* 37	YOFFSET */
	0x00, 0x96, /* 38	FATFINGERTRENTH */
	0x00, 0xFA, /* 39	MULTIFINGERSTRENTH */
	0x19,       /* 3A	AJFSINGALTHRES */
	0x00, 0x8C, /* 3B	XACCERLERATION */
	0x00, 0x64, /* 3C	YACCERLERATION */
};

/* Touchscreen Parameters Field Sizes (Writable: 0:Readonly; 1:Writable */
uint8_t cyttsp4_param_size[] = {
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
	1, /* 0B	RESERVED03 */
	2, /* 0C	RESERVED04 */
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
	2, /* 17	RESERVED05 */
	1, /* 18	LARGE_OBJ_THRES */
	1, /* 19	FAT_FINGER_THRES */
	1, /* 1A	RESERVED06 */
	1, /* 1B	RESERVED07 */
	1, /* 1C	LOW_POWER_INTV */
	1, /* 1D	REF_INTV */
	2, /* 1E	ACTIVE_MODE_TIMEOUT */
	1, /* 1F	WAKE_UP_SRC */
	2, /* 20	HOST_ALERT_PULSE_WIDTH */
	1, /* 21	RESERVED08 */
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
	2, /* 32	XDISPLAYSIZE */
	2, /* 33	YDISPLAYSIZE */
	1, /* 34	XFLIP */
	1, /* 35	YFLIP */
	2, /* 36	XOFFSET */
	2, /* 37	YOFFSET */
	2, /* 38	FATFINGERTRENTH */
	2, /* 39	MULTIFINGERSTRENTH */
	1, /* 3A	AJFSINGALTHRES */
	2, /* 3B	XACCERLERATION */
	2, /* 3C	YACCERLERATION */
};

#endif
