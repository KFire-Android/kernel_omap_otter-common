/*
 * drivers/i2c/chips/SenseTek/stk_i2c_als2207.h
 *
 * $Id: stk_i2c_als2207.h,v 1.0 2011/01/12 10:50:08 jsgood Exp $
 *
 * Copyright (C) 2010 Patrick Chang <patrick_chang@sitronix.com.tw>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	SenseTek/Sitronix Ambient Light Sensor Driver for ST2207
 *	based on stk_i2c_als2207.c
 */

#ifndef __STK_I2C_ALS2207_H
#define __STK_I2C_ALS2207_H


#define STK_ALS_I2C_SLAVE_ADDR1 (0x20>>1)
#define STK_ALS_I2C_SLAVE_ADDR2 (0x22>>1)


/* Define CMD */
#define STK_ALS_CMD1_GAIN(x) ((x)<<6)
#define STK_ALS_CMD1_IT(x) ((x)<<2)
#define STK_ALS_CMD1_WM(x) ((x)<<1)
#define STK_ALS_CMD1_SD(x) ((x)<<0)
#define STK_ALS_CMD2_FDIT(x) ((x)<<5)

#define STK_ALS_CMD1_GAIN_MASK 0xC0
#define STK_ALS_CMD1_IT_MASK 0x0C
#define STK_ALS_CMD1_WM_MASK 0x02
#define STK_ALS_CMD1_SD_MASK 0x1
#define STK_ALS_CMD2_FDIT_MASK 0xE

struct stkals22x7_data {
	struct i2c_client *client1;
	struct i2c_client *client2;
	struct input_dev* input_dev;
	uint32_t gain;
	uint32_t it;
	uint32_t fdit;
	unsigned int power_down_state : 1;
	int32_t als_lux_last;
	uint32_t als_delay;
	uint8_t bThreadRunning;
	uint8_t reg1,reg2;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

};

#define ALS_MIN_DELAY 250

#endif /*__STK_I2C_ALS_H*/
