/*
 * arch/arm/mach-omap2/board-44xx-tablet-sensors.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <linux/i2c/tsl2771.h>

#include <plat/i2c.h>

#include "board-44xx-tablet.h"
#include "mux.h"

#define OMAP4_TSL2771_INT_GPIO		184
#define OMAP4_TSL2771_PWR_GPIO		188

/* TSL2771 ALS/Prox Begin */

static void omap_tsl2771_power(int state)
{
	gpio_set_value(OMAP4_TSL2771_PWR_GPIO, state);
}

static void blaze_tablet_tsl2771_init(void)
{
	/* TO DO: Not sure what the use case of the proximity is on a tablet
	 * but the interrupt may need to be wakeable if and only if proximity
	 * is enabled but for now leave it alone */
	gpio_request(OMAP4_TSL2771_PWR_GPIO, "tsl2771_power");
	gpio_direction_output(OMAP4_TSL2771_PWR_GPIO, 0);

	gpio_request(OMAP4_TSL2771_INT_GPIO, "tsl2771_interrupt");
	gpio_direction_input(OMAP4_TSL2771_INT_GPIO);
}

/* TO DO: Need to create a interrupt threshold table here */

struct tsl2771_platform_data tsl2771_data = {
	.irq_flags	= (IRQF_TRIGGER_LOW | IRQF_ONESHOT),
	.flags		= (TSL2771_USE_ALS | TSL2771_USE_PROX),
	.def_enable			= 0x0,
	.als_adc_time 			= 0xdb,
	.prox_adc_time			= 0xff,
	.wait_time			= 0x00,
	.als_low_thresh_low_byte	= 0x0,
	.als_low_thresh_high_byte	= 0x0,
	.als_high_thresh_low_byte	= 0x0,
	.als_high_thresh_high_byte	= 0x0,
	.prox_low_thresh_low_byte	= 0x0,
	.prox_low_thresh_high_byte	= 0x0,
	.prox_high_thresh_low_byte	= 0x0,
	.prox_high_thresh_high_byte	= 0x0,
	.interrupt_persistence		= 0xf6,
	.config				= 0x00,
	.prox_pulse_count		= 0x03,
	.gain_control			= 0xE0,
	.glass_attn			= 0x01,
	.device_factor			= 0x34,
	.tsl2771_pwr_control		= omap_tsl2771_power,
};
/* TSL2771 ALS/Prox End */


static struct i2c_board_info __initdata blaze_tablet_i2c_bus4_sensor_info[] = {
	{
		I2C_BOARD_INFO(TSL2771_NAME, 0x39),
		.platform_data = &tsl2771_data,
		.irq = OMAP_GPIO_IRQ(OMAP4_TSL2771_INT_GPIO),
	},
};

int __init tablet_sensor_init(void)
{
	blaze_tablet_tsl2771_init();

	i2c_register_board_info(4, blaze_tablet_i2c_bus4_sensor_info,
		ARRAY_SIZE(blaze_tablet_i2c_bus4_sensor_info));

	return 0;
}

