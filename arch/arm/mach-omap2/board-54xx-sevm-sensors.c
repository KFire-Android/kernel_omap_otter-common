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


int __init sevm_sensor_init(void)
{

	/* Initialize Sensor modules here */

	return 0;
}
