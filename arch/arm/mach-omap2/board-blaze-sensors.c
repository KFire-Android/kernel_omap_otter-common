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

#include <linux/input/sfh7741.h>
#include <linux/i2c/cma3000.h>

#include <plat/i2c.h>

#include "board-blaze.h"
#include "mux.h"

#define OMAP4_SFH7741_SENSOR_OUTPUT_GPIO	184
#define OMAP4_SFH7741_ENABLE_GPIO		188
#define OMAP4_CMA3000ACCL_GPIO		186

static void omap_prox_activate(int state)
{
	gpio_set_value(OMAP4_SFH7741_ENABLE_GPIO , state);
}

static int omap_prox_read(void)
{
	int proximity;
	proximity = gpio_get_value(OMAP4_SFH7741_SENSOR_OUTPUT_GPIO);
#ifdef CONFIG_ANDROID
	/* Invert the output from the prox sensor for Android as 0 should
	be near and 1 should be far */
	return !proximity;
#else
	return proximity;
#endif
}

static void omap_sfh7741prox_init(void)
{
	int  error;
	struct omap_mux *prox_gpio_mux;
	bool wake_enable;

	error = gpio_request(OMAP4_SFH7741_SENSOR_OUTPUT_GPIO, "sfh7741");
	if (error < 0) {
		pr_err("%s: GPIO configuration failed: GPIO %d, error %d\n"
			, __func__, OMAP4_SFH7741_SENSOR_OUTPUT_GPIO, error);
		return ;
	}

	error = gpio_direction_input(OMAP4_SFH7741_SENSOR_OUTPUT_GPIO);
	if (error < 0) {
		pr_err("Proximity GPIO input configuration failed\n");
		goto fail1;
	}

	error = gpio_request(OMAP4_SFH7741_ENABLE_GPIO, "sfh7741");
	if (error < 0) {
		pr_err("failed to request GPIO %d, error %d\n",
			OMAP4_SFH7741_ENABLE_GPIO, error);
		goto fail1;
	}

	error = gpio_direction_output(OMAP4_SFH7741_ENABLE_GPIO , 0);
	if (error < 0) {
		pr_err("%s: GPIO configuration failed: GPIO %d,	error %d\n",
			__func__, OMAP4_SFH7741_ENABLE_GPIO, error);
		goto fail3;
	}

	prox_gpio_mux = omap_mux_get_gpio(OMAP4_SFH7741_SENSOR_OUTPUT_GPIO);
	wake_enable = omap_mux_get_wakeupenable(prox_gpio_mux);
	if (!wake_enable)
		omap_mux_set_wakeupenable(prox_gpio_mux);

	return;

fail3:
	gpio_free(OMAP4_SFH7741_ENABLE_GPIO);
fail1:
	gpio_free(OMAP4_SFH7741_SENSOR_OUTPUT_GPIO);
}

static struct sfh7741_platform_data omap_sfh7741_data = {
	.flags = SFH7741_WAKEABLE_INT,
	.irq_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	.prox_enable = 0,
	.activate_func = omap_prox_activate,
	.read_prox = omap_prox_read,
};

static struct platform_device blaze_proximity_device = {
	.name		= SFH7741_NAME,
	.id		= 1,
	.dev		= {
		.platform_data = &omap_sfh7741_data,
	},
};

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

static struct i2c_board_info __initdata blaze_bus3_sensor_boardinfo[] = {
	{
		I2C_BOARD_INFO("tmp105", 0x48),
	},
	{
		I2C_BOARD_INFO("bh1780", 0x29),
	},
};

static struct i2c_board_info __initdata blaze_bus4_sensor_boardinfo[] = {
	{
		I2C_BOARD_INFO("bmp085", 0x77),
	},
	{
		I2C_BOARD_INFO("hmc5843", 0x1e),
	},
	{
		I2C_BOARD_INFO("cma3000_accl", 0x1c),
		.platform_data = &cma3000_platform_data,
	},
};

int __init blaze_sensor_init(void)
{
	omap_sfh7741prox_init();
	omap_cma3000accl_init();

	i2c_register_board_info(3, blaze_bus3_sensor_boardinfo,
		ARRAY_SIZE(blaze_bus3_sensor_boardinfo));

	i2c_register_board_info(4, blaze_bus4_sensor_boardinfo,
		ARRAY_SIZE(blaze_bus4_sensor_boardinfo));

	platform_device_register(&blaze_proximity_device);

	return 0;
}
