/*
 * lvds-serlink_reg.h
 *
 * lvds based serial FPDLINK interface for onboard communication.
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

#ifndef LVDS_SERLINK_REG
#define LVDS_SERLINK_REG

#define SERLINK_I2C_DEVICE_ID	0x00
#define SERLINK_RESET		0x01
	#define SERLINK_RESET0		0
	#define SERLINK_RESET1		1

#define SERLINK_CONFIG_0	0x03
#define SERLINK_CONFIG_1	0x04
#define SERLINK_I2C_CONTROL	0x05
#define SERLINK_I2C_DES_ID	0x06
#define SERLINK_I2C_SLAVE_ID	0x07
#define SERLINK_I2C_SLAVE_ALIAS	0x08
#define SERLINK_CRC_LSB		0x0A
#define SERLINK_CRC_MSB		0x0B
#define SERLINK_GEN_STS		0x0C
	#define SERLINK_LINK_DETECT		0x1
	#define SERLINK_PCLK			0x4

/* GPIO settings*/
#define SERLINK_ID_GPIO0	0x0D
#define SERLINK_GPIO1_GPIO2	0x0E
#define SERLINK_GPIO3_GPIO4	0x0F
#define SERLINK_GPIO5_GPIO6	0x10
#define SERLINK_GPIO7_GPIO8	0x11

/* Setup SERLINK modes and configuration */
#define SERLINK_DATA_CTRL	0x12
#define SERLINK_MODE_STS	0x13
#define SERLINK_MODE_OSC	0x14

#define SERLINK_WDG_CTRL	0x16

/* I2C Timing */
#define SERLINK_I2C_CTRL	0x17
#define SERLINK_I2C_SCL_HT	0x18
#define SERLINK_I2C_SCL_LT	0x19

#define SERLINK_BC_ERR		0x1B

/* Pattern Generation */
#define SERLINK_PATTERN_GEN_CTRL	0x64
#define SERLINK_PATTERN_GEN_CONF	0x65
#define SERLINK_PATTERN_GEN_IND_ADDR	0x66
#define SERLINK_PATTERN_GEN_IND_DATA	0x67

/* Derializer */
#define SERLINK_RX_BKSV0	0x80
#define SERLINK_RX_BKSV1	0x81
#define SERLINK_RX_BKSV2	0x82
#define SERLINK_RX_BKSV3	0x83
#define SERLINK_RX_BKSV4	0x84
/* Serializer */
#define SERLINK_TX_KSV0		0x90
#define SERLINK_TX_KSV1		0x91
#define SERLINK_TX_KSV2		0x92
#define SERLINK_TX_KSV3		0x93
#define SERLINK_TX_KSV4		0x94

/* HDCP */
#define SERLINK_RX_BCAPS	0xA0
#define SERLINK_RX_BSTS0	0xA1
#define SERLINK_RX_BSTS1	0xA2
#define SERLINK_KSVFIFO		0xA3
#define SERLINK_HDCP_DBG	0xC0
#define SERLINK_HDCP_CFG	0xC2
#define SERLINK_HDCP_CTL	0xC3
#define SERLINK_HDCP_STS	0xC4
#define SERLINK_HDCP_ICR	0xC6
#define SERLINK_HDCP_ISR	0xC7

#define SERLINK_HDCP_TX_ID0	0xF0
#define SERLINK_HDCP_TX_ID1	0xF1
#define SERLINK_HDCP_TX_ID2	0xF2
#define SERLINK_HDCP_TX_ID3	0xF3
#define SERLINK_HDCP_TX_ID4	0xF4
#define SERLINK_HDCP_TX_ID5	0xF5

#endif /* LVDS_SERLINK_REG */
