/*
 * include/linux/i2c/twl6030-gpadc.h
 *
 * TWL6030 GPADC module driver header
 *
 * Copyright (C) 2009 Texas Instruments Inc.
 * Nishant Kamat <nskamat@ti.com>
 *
 * Based on twl4030-madc.h
 * Copyright (C) 2008 Nokia Corporation
 * Mikko Ylinen <mikko.k.ylinen@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef _TWL6030_GPADC_H
#define _TWL6030_GPADC_H

struct twl6030_gpadc_conversion_method {
	u8 sel;
	u8 rbase;
	u8 ctrl;
	u8 enable;
};

#define TWL6030_GPADC_MAX_CHANNELS 17

/*
 * raw_code - raw adc value
 * raw_channel_value - adc * channel gain
 * code - calibrated adc value
 */
struct twl6030_value {
	int raw_code;
	int raw_channel_value;
	int code;
};

struct twl6030_gpadc_request {
	u32 channels;
	u16 do_avg;
	u16 method;
	u16 type;
	int active;
	int result_pending;
	int rbuf[TWL6030_GPADC_MAX_CHANNELS];
	void (*func_cb)(int len, int channels, int *buf);
	struct twl6030_value buf[TWL6030_GPADC_MAX_CHANNELS];
};

enum conversion_methods {
	TWL6030_GPADC_RT,
	TWL6030_GPADC_SW2,
	TWL6030_GPADC_NUM_METHODS
};

enum sample_type {
	TWL6030_GPADC_WAIT,
	TWL6030_GPADC_IRQ_ONESHOT,
	TWL6030_GPADC_IRQ_REARM
};

#define TWL6030_GPADC_CTRL		0x00    /* 0x2e */

#define TWL6030_GPADC_RTSELECT_LSB	0x02    /* 0x30 */
#define TWL6030_GPADC_RTSELECT_ISB	0x03
#define TWL6030_GPADC_RTSELECT_MSB	0x04

#define TWL6030_GPADC_CTRL_P1		0x05
#define TWL6030_GPADC_CTRL_P2		0x06
#define TWL6030_GPADC_CTRL_P1_SP1	(1 << 3)
#define TWL6030_GPADC_CTRL_P1_EOCRT	(1 << 2)
#define TWL6030_GPADC_CTRL_P1_EOCP1	(1 << 1)
#define TWL6030_GPADC_CTRL_P1_BUSY	(1 << 0)

#define TWL6030_GPADC_CTRL_P2_SP2	(1 << 2)
#define TWL6030_GPADC_CTRL_P2_EOCP2	(1 << 1)
#define TWL6030_GPADC_CTRL_P1_BUSY	(1 << 0)

#define TWL6030_GPADC_EOC_SW		(1 << 1)
#define TWL6030_GPADC_BUSY		(1 << 0)

#define TWL6030_GPADC_RTCH0_LSB		(0x07)
#define TWL6030_GPADC_GPCH0_LSB		(0x29)

/* Fixed channels */
#define TWL6030_GPADC_CTRL_TEMP1_EN	(1 << 0)    /* input ch 1 */
#define TWL6030_GPADC_CTRL_TEMP2_EN	(1 << 1)    /* input ch 4 */
#define TWL6030_GPADC_CTRL_SCALER_EN	(1 << 2)    /* input ch 2 */
#define TWL6030_GPADC_CTRL_SCALER_DIV4	(1 << 3)
#define TWL6030_GPADC_CTRL_SCALER_EN_CH11	(1 << 4)    /* input ch 11 */
#define TWL6030_GPADC_CTRL_TEMP1_EN_MONITOR	(1 << 5)
#define TWL6030_GPADC_CTRL_TEMP2_EN_MONITOR	(1 << 6)
#define TWL6030_GPADC_CTRL_ISOURCE_EN		(1 << 7)


#define TWL6030_GPADC_IOC_MAGIC '`'
#define TWL6030_GPADC_IOCX_ADC_RAW_READ	_IO(TWL6030_GPADC_IOC_MAGIC, 0)

struct twl6030_gpadc_user_parms {
	int channel;
	int status;
	u16 result;
};

int twl6030_gpadc_conversion(struct twl6030_gpadc_request *conv);

#endif
