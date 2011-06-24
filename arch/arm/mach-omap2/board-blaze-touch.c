/*
 * arch/arm/mach-omap2/board-blaze-touch.c
 *
 * Copyright (C) 2011 Texas Instruments
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
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "board-blaze.h"
#include <plat/i2c.h>
#include <plat/syntm12xx.h>

#include "mux.h"

#define OMAP4_TOUCH_IRQ_1		35
#define OMAP4_TOUCH_IRQ_2		36

static char *tm12xx_idev_names[] = {
	"syn_tm12xx_ts_1",
	"syn_tm12xx_ts_2",
	"syn_tm12xx_ts_3",
	"syn_tm12xx_ts_4",
	"syn_tm12xx_ts_5",
	"syn_tm12xx_ts_6",
	NULL,
};

static u8 tm12xx_button_map[] = {
	KEY_F1,
	KEY_F2,
};

static struct tm12xx_ts_platform_data tm12xx_platform_data[] = {
	{ /* Primary Controller */
		.gpio_intr = OMAP4_TOUCH_IRQ_1,
		.idev_name = tm12xx_idev_names,
		.button_map = tm12xx_button_map,
		.num_buttons = ARRAY_SIZE(tm12xx_button_map),
		.repeat = 0,
		.swap_xy = 1,
		.controller_num = 0,
	/* Android does not have touchscreen as wakeup source */
#if !defined(CONFIG_ANDROID)
		.suspend_state = SYNTM12XX_ON_ON_SUSPEND,
#else
		.suspend_state = SYNTM12XX_SLEEP_ON_SUSPEND,
#endif
	},
	{ /* Secondary Controller */
		.gpio_intr = OMAP4_TOUCH_IRQ_2,
		.idev_name = tm12xx_idev_names,
		.button_map = tm12xx_button_map,
		.num_buttons = ARRAY_SIZE(tm12xx_button_map),
		.repeat = 0,
		.swap_xy = 1,
		.controller_num = 1,
	/* Android does not have touchscreen as wakeup source */
#if !defined(CONFIG_ANDROID)
		.suspend_state = SYNTM12XX_ON_ON_SUSPEND,
#else
		.suspend_state = SYNTM12XX_SLEEP_ON_SUSPEND,
#endif
	},
};

static struct i2c_board_info __initdata blaze_i2c_2_touch_boardinfo[] = {
	{
		I2C_BOARD_INFO("tm12xx_ts_primary", 0x4b),
		.platform_data = &tm12xx_platform_data[0],
	},
};

static struct i2c_board_info __initdata blaze_i2c_3_touch_boardinfo[] = {
	{
		I2C_BOARD_INFO("tm12xx_ts_secondary", 0x4b),
		.platform_data = &tm12xx_platform_data[1],
	},
};

int __init blaze_touch_init(void)
{

	i2c_register_board_info(2, blaze_i2c_2_touch_boardinfo,
		ARRAY_SIZE(blaze_i2c_2_touch_boardinfo));

	i2c_register_board_info(3, blaze_i2c_3_touch_boardinfo,
		ARRAY_SIZE(blaze_i2c_3_touch_boardinfo));

	return 0;
}
