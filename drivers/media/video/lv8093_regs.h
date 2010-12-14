/*
 * lv8093_regs.h
 *
 * Copyright (C) 2008-2009 Texas Instruments.
 * Copyright (C) 2009 Hewlett-Packard.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Register defines for Lens piezo-actuator device
 *
 */
#ifndef LV8093_REGS_H
#define LV8093_REGS_H

#include <media/v4l2-int-device.h>

#define LV8093_I2C_RETRY_COUNT		5

#define CAMAF_LV8093_DISABLE		0x1
#define CAMAF_LV8093_ENABLE	    	0x0
#define CAMAF_LV8093_DRVPLS_REG		0x0
#define CAMAF_LV8093_CTL_REG		0x1
#define CAMAF_LV8093_RST_REG		0x2
#define CAMAF_LV8093_GTAS_REG		0x3
#define CAMAF_LV8093_GTBR_REG		0x4
#define CAMAF_LV8093_GTBS_REG		0x5
#define CAMAF_LV8093_STP_REG		0x6
#define CAMAF_LV8093_MOV_REG		0x7
#define CAMAF_LV8093_MAC_DIR        0x80
#define CAMAF_LV8093_INF_DIR        0x00
#define CAMAF_LV8093_GATE0          0x00
#define CAMAF_LV8093_GATE1          0x80
#define CAMAF_LV8093_ENIN           0x20
#define CAMAF_LV8093_CKSEL_ONE      0x18
#define CAMAF_LV8093_CKSEL_HALF     0x08
#define CAMAF_LV8093_CKSEL_QTR      0x00
#define CAMAF_LV8093_RET2           0x00
#define CAMAF_LV8093_RET1           0x02
#define CAMAF_LV8093_RET3           0x04
#define CAMAF_LV8093_RET4           0x06
#define CAMAF_LV8093_INIT_OFF       0x01
#define CAMAF_LV8093_INIT_ON        0x00
#define CAMAF_LV8093_BUSY           0x80
#define CAMAF_LV8093_REGDATA(REG, DATA)  (((REG) << 8) | (DATA))

#define CAMAF_LV8093_POWERDN(ARG)	(((ARG) & 0x1) << 15)
#define CAMAF_LV8093_POWERDN_R(ARG)	(((ARG) >> 15) & 0x1)

#define CAMAF_LV8093_DATA(ARG)		(((ARG) & 0xFF) << 6)
#define CAMAF_LV8093_DATA_R(ARG)	(((ARG) >> 6) & 0xFF)
#define CAMAF_FREQUENCY_EQ1(mclk)     	((u16)(mclk/16000))

/* State of lens */
#define LENS_DETECTED 1
#define LENS_NOT_DETECTED 0

/* Focus control values */
#define LV8093_MAX_RELATIVE_STEP	127

/* Initialization Mode Settings */
#define LV8093_TIME_GATEA	23		/* First pulse width. */
#define LV8093_TIME_OFF 	2		/* Off time between pulses. */
#define LV8093_TIME_GATEB	29		/* Second pulse width. */
#define LV8093_STP 			24		/* Pulse repetitions. */
/* Numbers of clock periods per cycle: */
/* 18MHz clock, period = 55.6 nsec */
#define LV8093_CLK_PER_PERIOD	104

#endif /* End of of LV8093_REGS_H */

