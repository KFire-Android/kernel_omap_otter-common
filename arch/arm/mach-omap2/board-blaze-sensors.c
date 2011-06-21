/*
 * arch/arm/mach-omap2/board-blaze-sensors.c
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <DMurphy@TI.com>
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <linux/i2c/cma3000.h>

#include <plat/i2c.h>

#include "board-blaze.h"
#include "mux.h"

#define OMAP4_CMA3000ACCL_GPIO		186

static struct cma3000_platform_data cma3000_platform_data = {
	.def_poll_rate = 200,
	.fuzz_x = 25,
	.fuzz_y = 25,
	.fuzz_z = 25,
	.g_range = CMARANGE_8G,
	.mode = CMAMODE_MEAS400,
	.mdthr = 0x8,
	.mdfftmr = 0x33,
	.ffthr = 0x8,
	.irqflags = IRQF_TRIGGER_HIGH,
};

static void omap_cma3000accl_init(void)
{
	if (gpio_request(OMAP4_CMA3000ACCL_GPIO, "Accelerometer") < 0) {
		pr_err("Accelerometer GPIO request failed\n");
		return;
	}
	gpio_direction_input(OMAP4_CMA3000ACCL_GPIO);
}

static struct i2c_board_info __initdata blaze_bus4_sensor_boardinfo[] = {
	{
		I2C_BOARD_INFO("cma3000_accl", 0x1c),
		.platform_data = &cma3000_platform_data,
	},
};

int __init blaze_sensor_init(void)
{
	omap_cma3000accl_init();

	i2c_register_board_info(4, blaze_bus4_sensor_boardinfo,
		ARRAY_SIZE(blaze_bus4_sensor_boardinfo));

	return 0;
}
