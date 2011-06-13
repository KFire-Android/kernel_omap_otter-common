/*
 * arch/arm/mach-omap2/board-blaze-keypad.c
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

#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/platform_device.h>

#include <plat/omap4-keypad.h>

#include "board-blaze.h"
#include "mux.h"

static struct platform_device blaze_keypad_led = {
	.name	=	"keypad_led",
	.id	=	-1,
	.dev	= {
		.platform_data = NULL,
	},
};

static struct platform_device *blaze_led_devices[] __initdata = {
	&blaze_keypad_led,
};

int __init blaze_keypad_init(void)
{
	platform_add_devices(blaze_led_devices, ARRAY_SIZE(blaze_led_devices));

	return 0;
}
