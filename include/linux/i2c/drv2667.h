/*
 * drv2667.h
 * DRV2667 Piezo-haptic driver
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Dan Murphy <DMurphy@ti.com>
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

#ifndef _LINUX_DRV2667_I2C_H
#define _LINUX_DRV2667_I2C_H

#define DRV2667_NAME	"drv2667"

/* Contol registers */
#define DRV2667_STATUS	0x0
#define DRV2667_CTRL_1	0x1
#define DRV2667_CTRL_2	0x2
/* Waveform sequencer */
#define DRV2667_WV_SEQ_0	0x3
#define DRV2667_WV_SEQ_1	0x4
#define DRV2667_WV_SEQ_2	0x5
#define DRV2667_WV_SEQ_3	0x6
#define DRV2667_WV_SEQ_4	0x7
#define DRV2667_WV_SEQ_5	0x8
#define DRV2667_WV_SEQ_6	0x9
#define DRV2667_WV_SEQ_7	0xA
#define DRV2667_FIFO		0xB
#define DRV2667_PAGE		0xFF

/* RAM fields */
#define DRV2667_RAM_HDR_SZ	0x0
/* RAM Header addresses */
#define DRV2667_RAM_START_HI	0x1
#define DRV2667_RAM_START_LO	0x2
#define DRV2667_RAM_STOP_HI	0x3
#define DRV2667_RAM_STOP_LO	0x4
#define DRV2667_RAM_REPEAT_CT	0x5
/* RAM data addresses */
#define DRV2667_RAM_AMP		0x6
#define DRV2667_RAM_FREQ	0x7
#define DRV2667_RAM_DURATION	0x8
#define DRV2667_RAM_ENVELOPE	0x9

struct drv2667_pattern_data {
	u8 wave_seq;
	u8 page_num;
	u8 hdr_size;
	u8 start_addr_upper;
	u8 start_addr_lower;
	u8 stop_addr_upper;
	u8 stop_addr_lower;
	u8 repeat_count;
	u8 amplitude;
	u8 frequency;
	u8 duration;
	u8 envelope;
};

struct drv2667_pattern_array {
	struct drv2667_pattern_data *pattern_data;
	int num_of_patterns;
};

struct drv2667_platform_data {
	struct drv2667_pattern_array drv2667_pattern_data;
	int initial_vibrate;
};

#endif
