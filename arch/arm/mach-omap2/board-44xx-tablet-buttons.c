/*
 * Button and Button LED support OMAP44xx tablet.
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>

#include <plat/omap_apps_brd_id.h>

#include "mux.h"
#include "board-44xx-tablet.h"

#define TABLET2_GREEN_LED_GPIO		174
#define TABLET2_GREEN_DBG2_LED_GPIO	173

static struct gpio_led tablet_gpio_leds[] = {
	{
		.name	= "omap4:green:debug2",
		.gpio	= 7,
	},
	{
		.name	= "omap4:green:debug4",
		.gpio	= 50,
	},
	{
		.name	= "blue",
		.default_trigger = "timer",
		.gpio	= 169,
	},
	{
		.name	= "red",
		.default_trigger = "timer",
		.gpio	= 170,
	},
	{
		.name	= "green",
		.default_trigger = "timer",
		.gpio	= 139,
	},

};

static struct gpio_led_platform_data tablet_led_data = {
	.leds	= tablet_gpio_leds,
	.num_leds = ARRAY_SIZE(tablet_gpio_leds),
};

static struct led_pwm tablet_pwm_leds[] = {
	{
		.name		= "omap4:green:chrg",
		.pwm_id		= 1,
		.max_brightness	= 255,
		.pwm_period_ns	= 7812500,
	},
};

static struct led_pwm_platform_data tablet_pwm_data = {
	.num_leds	= ARRAY_SIZE(tablet_pwm_leds),
	.leds		= tablet_pwm_leds,
};

static struct platform_device tablet_leds_pwm = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev	= {
		.platform_data = &tablet_pwm_data,
	},
};

static struct platform_device tablet_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &tablet_led_data,
	},
};

/* GPIO_KEY for Tablet */
static struct gpio_keys_button tablet_gpio_keys_buttons[] = {
	[0] = {
		.code			= KEY_VOLUMEUP,
		.gpio			= 43,
		.desc			= "SW1",
		.active_low		= 1,
	},
	[1] = {
		.code			= KEY_HOME,
		.gpio			= 46,
		.desc			= "SW2",
		.active_low		= 1,
		.wakeup			= 1,
	},
	[2] = {
		.code			= KEY_VOLUMEDOWN,
		.gpio			= 47,
		.desc			= "SW3",
		.active_low		= 1,
		},
	};

static struct gpio_keys_platform_data tablet_gpio_keys = {
	.buttons		= tablet_gpio_keys_buttons,
	.nbuttons		= ARRAY_SIZE(tablet_gpio_keys_buttons),
	.rep			= 0,
};

static struct platform_device tablet_gpio_keys_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &tablet_gpio_keys,
	},
};

static struct platform_device *tablet_devices[] __initdata = {
	&tablet_leds_gpio,
	&tablet_leds_pwm,
	&tablet_gpio_keys_device,
};

int __init tablet_button_init(void)
{
	if (omap_is_board_version(OMAP4_TABLET_2_0) ||
		omap_is_board_version(OMAP4_TABLET_2_1) ||
		omap_is_board_version(OMAP4_TABLET_2_1_1) ||
		omap_is_board_version(OMAP4_TABLET_2_1_2)) {

		omap_mux_init_gpio(TABLET2_GREEN_DBG2_LED_GPIO,
			OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT);
		omap_mux_init_gpio(TABLET2_GREEN_LED_GPIO,
			OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT);
		tablet_gpio_leds[0].gpio = TABLET2_GREEN_DBG2_LED_GPIO;
		tablet_gpio_leds[4].gpio = TABLET2_GREEN_LED_GPIO;
	}

	if (omap_board_uses_hsic()) {
		tablet_gpio_leds[2].gpio = 104;
		tablet_gpio_leds[3].gpio = 36;
	}

	platform_add_devices(tablet_devices, ARRAY_SIZE(tablet_devices));
	return 0;

}
