/*
 * arch/arm/mach-omap2/board-omap5evm-connectivity.c
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Pradeep Gurumath <pradeepgurumath@ti.com>
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
#include "mux.h"

#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <linux/wl12xx.h>

#define GPIO_WIFI_PMENA     140
#define GPIO_WIFI_IRQ       9

static struct regulator_consumer_supply omap5_sdp5430_vmmc3_supply = {
	.supply         = "vmmc",
	.dev_name       = "omap_hsmmc.2",
};

static struct regulator_init_data sdp5430_vmmc3 = {
		.constraints            = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &omap5_sdp5430_vmmc3_supply,
};

static struct fixed_voltage_config sdp5430_vwlan = {
	.supply_name            = "vwl1271",
	.microvolts             = 1800000, /* 1.8V */
	.gpio                   = GPIO_WIFI_PMENA,
	.startup_delay          = 70000, /* 70msec */
	.enable_high            = 1,
	.enabled_at_boot        = 0,
	.init_data              = &sdp5430_vmmc3,
};

static struct platform_device omap_vwlan_device = {
	.name           = "reg-fixed-voltage",
	.id             = 2,
	.dev = {
		.platform_data = &sdp5430_vwlan,
	},
};

static struct wl12xx_platform_data omap5_sdp5430_wlan_data __initdata = {
	.board_ref_clock    = WL12XX_REFCLOCK_26,
	.board_tcxo_clock   = WL12XX_TCXOCLOCK_26,
};

static void __init omap5_sdp5430_wifi_mux_init(void)
{
	omap_mux_init_gpio(GPIO_WIFI_IRQ, OMAP_PIN_INPUT);

	omap_mux_init_gpio(GPIO_WIFI_PMENA, OMAP_PIN_OUTPUT);

	omap_mux_init_signal("wlsdio_cmd.wlsdio_cmd",
					OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("wlsdio_clk.wlsdio_clk",
					OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("wlsdio_data0.wlsdio_data0",
					OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("wlsdio_data1.wlsdio_data1",
					OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("wlsdio_data2.wlsdio_data2",
					OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("wlsdio_data3.wlsdio_data3",
					OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
}

void __init omap5_sdp5430_wifi_init(void)
{
	int ret;

	omap5_sdp5430_wifi_mux_init();

	ret = gpio_request_one(GPIO_WIFI_IRQ, GPIOF_IN, "wlan");
	if (ret) {
		printk(KERN_INFO "wlan: IRQ gpio request failure in board file\n");
		return;
	}

	omap5_sdp5430_wlan_data.irq = gpio_to_irq(GPIO_WIFI_IRQ);

	ret = wl12xx_set_platform_data(&omap5_sdp5430_wlan_data);
	if (ret) {
		pr_err("Error setting wl12xx data\n");
		return;
	}

	ret = platform_device_register(&omap_vwlan_device);
	if (ret)
		pr_err("Error registering wl12xx device: %d\n", ret);
}

int __init omap5sevm_connectivity_init(void)
{
	omap5_sdp5430_wifi_init();

	return 0;
}
