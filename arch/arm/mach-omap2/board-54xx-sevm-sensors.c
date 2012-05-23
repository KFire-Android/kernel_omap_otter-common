/*
 * arch/arm/mach-omap2/board-54xx-sevm-sensors.c
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Leed Aguilar <leed.aguilar@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/i2c/tsl2771.h>

#include <plat/i2c.h>

#define OMAP5_TSL2771_INT_GPIO		149

struct tsl2771_platform_data tsl2771_data = {
	.irq_flags      = (IRQF_TRIGGER_LOW | IRQF_ONESHOT),
	.flags          = (TSL2771_USE_ALS | TSL2771_USE_PROX),
	.def_enable                     = 0x0,
	.als_adc_time                   = 0xdb,
	.prox_adc_time                  = 0xff,
	.wait_time                      = 0x00,
	.als_low_thresh_low_byte        = 0x0,
	.als_low_thresh_high_byte       = 0x0,
	.als_high_thresh_low_byte       = 0x0,
	.als_high_thresh_high_byte      = 0x0,
	.prox_low_thresh_low_byte       = 0x0,
	.prox_low_thresh_high_byte      = 0x0,
	.prox_high_thresh_low_byte      = 0x0,
	.prox_high_thresh_high_byte     = 0x0,
	.interrupt_persistence          = 0xf6,
	.config                         = 0x00,
	.prox_pulse_count               = 0x03,
	.gain_control                   = 0xE0,
	.glass_attn                     = 0x01,
	.device_factor                  = 0x34,
};

static struct i2c_board_info __initdata omap5evm_sensor_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("bmp085", 0x77),
	},
	{
		I2C_BOARD_INFO("tsl2771", 0x39),
		.platform_data = &tsl2771_data,
		.irq = OMAP5_TSL2771_INT_GPIO,
	},
};

int __init sevm_sensor_init(void)
{
	i2c_register_board_info(2, omap5evm_sensor_i2c2_boardinfo,
		ARRAY_SIZE(omap5evm_sensor_i2c2_boardinfo));

	return 0;
}
