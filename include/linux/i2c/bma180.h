/*
 * bma180.h
 * BMA-180 Accelerometer driver
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Jorge Bustamante <jbustamante@ti.com>
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

#ifndef _LINUX_BMA180_I2C_H
#define _LINUX_BMA180_I2C_H

#define BMA_GRANGE_1G		0x0
#define BMA_GRANGE_1_5G		0x1
#define BMA_GRANGE_2G		0x2
#define BMA_GRANGE_3G		0x3
#define BMA_GRANGE_4G		0x4
#define BMA_GRANGE_8G		0x5
#define BMA_GRANGE_16G		0x6

#define BMA_BW_10HZ		0x0
#define BMA_BW_20HZ		0x1
#define BMA_BW_40HZ		0x2
#define BMA_BW_75HZ		0x3
#define BMA_BW_150HZ		0x4
#define BMA_BW_300HZ		0x5
#define BMA_BW_600HZ		0x6
#define BMA_BW_1200HZ		0x7
#define BMA_BW_HP_1HZ		0x8
#define BMA_BW_BP_0_2HZ_300HZ   0x9

#define BMA_MODE_LOW_NOISE		0x0
#define BMA_MODE_SUPER_LOW_NOISE	0x1
#define BMA_MODE_ULTRA_LOW_NOISE	0x2
#define BMA_MODE_LOW_POWER		0x3

#define BMA_BITMODE_14BITS		0x2000
#define BMA_BITMODE_12BITS		0x0800

#define BMA180_SLEEP			0x02

struct bma180accel_platform_data {
	uint8_t method;
	uint32_t def_poll_rate;
	uint16_t fuzz_x;
	uint16_t fuzz_y;
	uint16_t fuzz_z;
	uint8_t ctrl_reg0;
	uint8_t ctrl_reg1;
	uint8_t ctrl_reg2;
	uint8_t ctrl_reg3;
	uint8_t ctrl_reg4;

	uint16_t bandwidth;
	uint8_t g_range;
	uint8_t mode;
	int bit_mode;
	uint8_t smp_skip;
};

#endif
