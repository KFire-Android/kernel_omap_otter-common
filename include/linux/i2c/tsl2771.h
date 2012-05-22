/*
 * tsl2771.h
 * TSL2771 ALS and Proximity driver
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

#ifndef _LINUX_TSL2771_I2C_H
#define _LINUX_TSL2771_I2C_H

#define TSL2771_NAME	"tsl2771"

/* TSL2771 Read/Write registers */
#define TSL2771_ENABLE	0x00
#define TSL2771_ATIME	0x01
#define TSL2771_PTIME	0x02
#define TSL2771_WTIME	0x03
#define TSL2771_AILTL	0x04
#define TSL2771_AILTH	0x05
#define TSL2771_AIHTL	0x06
#define TSL2771_AIHTH	0x07
#define TSL2771_PILTL	0x08
#define TSL2771_PILTH	0x09
#define TSL2771_PIHTL	0x0a
#define TSL2771_PIHTH	0x0b
#define TSL2771_PERS	0x0c
#define TSL2771_CONFIG	0x0d
#define TSL2771_PPCOUNT	0x0e
#define TSL2771_CONTROL	0x0f

#define TSL2771_USE_ALS		(1 << 0)
#define TSL2771_USE_PROX	(1 << 1)

struct tsl2771_platform_data {
	int irq_flags;
	int flags;
	uint8_t def_enable;
	uint8_t als_adc_time;
	uint8_t prox_adc_time;
	uint8_t wait_time;
	uint8_t als_low_thresh_low_byte;
	uint8_t als_low_thresh_high_byte;
	uint8_t als_high_thresh_low_byte;
	uint8_t als_high_thresh_high_byte;
	uint8_t prox_low_thresh_low_byte;
	uint8_t prox_low_thresh_high_byte;
	uint8_t prox_high_thresh_low_byte;
	uint8_t prox_high_thresh_high_byte;
	uint8_t interrupt_persistence;
	uint8_t config;
	uint8_t prox_pulse_count;
	uint8_t gain_control;

	int glass_attn;
	int device_factor;

	void (*tsl2771_pwr_control)(int state);
};

#endif
