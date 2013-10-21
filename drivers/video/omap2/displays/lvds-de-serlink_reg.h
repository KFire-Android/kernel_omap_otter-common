/*
 * lvds-de-serlink_reg.h
 *
 * lvds based de-serial FPDLINK interface for onboard communication.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Dandawate Saket <dsaket@ti.com>
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

#ifndef LVDS_DSERLINK_REG
#define LVDS_DSERLINK_REG

#define DSERLINK_I2C_DEVICE_ID	0x00
#define DSERLINK_RESET			0x01
	#define DSERLINK_RESET0		0
	#define DSERLINK_RESET1		1
	#define DSERLINK_BC_EN		2

#define DSERLINK_CONFIG0		0x02
#define DSERLINK_CONFIG1		0x03
#define DSERLINK_BBC_WDT		0x04
#define DSERLINK_I2C_CTRL1		0x05
#define DSERLINK_I2C_CTRL2		0x06
#define DSERLINK_SER_I2C_ID		0x07
#define DSERLINK_SLAVE_ID0		0x08
#define DSERLINK_SLAVE_ID1		0x09
#define DSERLINK_SLAVE_ID2		0x0A
#define DSERLINK_SLAVE_ID3		0x0B
#define DSERLINK_SLAVE_ID4		0x0C
#define DSERLINK_SLAVE_ID5		0x0D
#define DSERLINK_SLAVE_ID6		0x0E
#define DSERLINK_SLAVE_ID7		0x0F

#define DSERLINK_SLAVE_AL_ID0	0x10
#define DSERLINK_SLAVE_AL_ID1	0x11
#define DSERLINK_SLAVE_AL_ID2	0x12
#define DSERLINK_SLAVE_AL_ID3	0x13
#define DSERLINK_SLAVE_AL_ID4	0x14
#define DSERLINK_SLAVE_AL_ID5	0x15
#define DSERLINK_SLAVE_AL_ID6	0x16
#define DSERLINK_SLAVE_AL_ID7	0x17

#define DSERLINK_MB0			0x18
#define DSERLINK_MB1			0x19
#define DSERLINK_FR_CNT			0x1B

#define DSERLINK_GN_STS			0x1C
#define DSERLINK_GPIO_0			0x1D
#define DSERLINK_GPIO_1_2		0x1E
#define DSERLINK_GPIO_3			0x1F
#define DSERLINK_GPIO_5_6		0x20
#define DSERLINK_GPIO_7_8		0x21

#define DSERLINK_DATA_CTL		0x22
#define DSERLINK_RX_STS			0x23
#define DSERLINK_BIST_CTL		0x24
#define DSERLINK_BIST_ERR		0x25
#define DSERLINK_SCL_HT			0x26
#define DSERLINK_SCL_LT			0x27

#define DSERLINK_DATA_CTL2		0x28
#define DSERLINK_FRC_CTL		0x29
#define DSERLINK_WB_CTL			0x2A
#define DSERLINK_I2S_CTL		0x2B

#define DSERLINK_AEQ_CTL		0x35
#define DSERLINK_CLK_EN			0x39
#define DSERLINK_I2S_DIVSEL		0x3A
#define DSERLINK_AEQ_STS		0x3B
#define DSERLINK_AEQ_BYPASS		0x44
#define DSERLINK_AEQ_MIN_MAX	0x45
#define DSERLINK_MAP_SEL		0x49

#define DSERLINK_LOOP_DRV		0x56
#define DSERLINK_PATTERN_GEN	0x64
#define DSERLINK_PATTERN_CONF	0x65

#endif /* LVDS_SERLINK_REG */
