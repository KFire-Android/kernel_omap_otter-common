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

#include <plat/i2c.h>

static struct i2c_board_info __initdata omap5evm_sensor_i2c2_boardinfo[] = {
	{
		I2C_BOARD_INFO("bmp085", 0x77),
	},
};

int __init sevm_sensor_init(void)
{
	i2c_register_board_info(2, omap5evm_sensor_i2c2_boardinfo,
		ARRAY_SIZE(omap5evm_sensor_i2c2_boardinfo));

	return 0;
}
