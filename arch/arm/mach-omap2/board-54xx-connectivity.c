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

/* for TI Shared Transport devices */
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <plat/omap-serial.h>
#include <linux/wakelock.h>

#include "board-54xx-sevm.h"

#define WILINK_UART_DEV_NAME "/dev/ttyO4"
#define OMAP5_BT_NSHUTDOWN_GPIO	142

#define GPIO_WIFI_PMENA     140
#define GPIO_WIFI_IRQ       9

static struct regulator_consumer_supply omap5_evm_vmmc3_supply = {
	.supply         = "vmmc",
	.dev_name       = "omap_hsmmc.2",
};

static struct regulator_init_data evm_vmmc3 = {
		.constraints            = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &omap5_evm_vmmc3_supply,
};

static struct fixed_voltage_config evm_vwlan = {
	.supply_name            = "vwl1271",
	.microvolts             = 1800000, /* 1.8V */
	.gpio                   = GPIO_WIFI_PMENA,
	.startup_delay          = 70000, /* 70msec */
	.enable_high            = 1,
	.enabled_at_boot        = 0,
	.init_data              = &evm_vmmc3,
};

static struct platform_device omap_vwlan_device = {
	.name           = "reg-fixed-voltage",
	.id             = 2,
	.dev = {
		.platform_data = &evm_vwlan,
	},
};

static struct wl12xx_platform_data omap5_evm_wlan_data __initdata = {
	.board_ref_clock    = WL12XX_REFCLOCK_26,
	.board_tcxo_clock   = WL12XX_TCXOCLOCK_26,
};

static void __init omap5_evm_wifi_mux_init(void)
{
	omap_mux_init_gpio(GPIO_WIFI_IRQ, OMAP_PIN_INPUT_PULLUP | OMAP_WAKEUP_EN);

	omap_mux_init_gpio(GPIO_WIFI_PMENA, OMAP_PIN_OUTPUT |
			OMAP_PIN_INPUT_PULLUP);

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

static void __init omap5_evm_wifi_init(void)
{
	int ret;

	omap5_evm_wifi_mux_init();

	ret = gpio_request_one(GPIO_WIFI_IRQ, GPIOF_IN, "wlan");
	if (ret) {
		printk(KERN_INFO "wlan: IRQ gpio request failure in board file\n");
		return;
	}

	omap5_evm_wlan_data.irq = gpio_to_irq(GPIO_WIFI_IRQ);

	ret = wl12xx_set_platform_data(&omap5_evm_wlan_data);
	if (ret) {
		pr_err("Error setting wl12xx data\n");
		return;
	}

	ret = platform_device_register(&omap_vwlan_device);
	if (ret)
		pr_err("Error registering wl12xx device: %d\n", ret);
}

/* TODO: handle suspend/resume here.
 * Upon every suspend, make sure the wilink chip is capable
 * enough to wake-up the OMAP host.
 */
static int plat_wlink_kim_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	return 0;
}

static int plat_wlink_kim_resume(struct platform_device *pdev)
{
	return 0;
}

static bool uart_req;
static struct wake_lock st_wk_lock;
/* Call the uart disable of serial driver */
static int plat_uart_disable(struct kim_data_s *un_used)
{
	int port_id = 0;
	int err = 0;
	if (uart_req) {
		sscanf(WILINK_UART_DEV_NAME, "/dev/ttyO%d", &port_id);
		err = omap_serial_ext_uart_disable(port_id);
		if (!err)
			uart_req = false;
	}
	wake_unlock(&st_wk_lock);
	return err;
}

/* Call the uart enable of serial driver */
static int plat_uart_enable(struct kim_data_s *un_used)
{
	int port_id = 0;
	int err = 0;
	if (!uart_req) {
		sscanf(WILINK_UART_DEV_NAME, "/dev/ttyO%d", &port_id);
		err = omap_serial_ext_uart_enable(port_id);
		if (!err)
			uart_req = true;
	}
	wake_lock(&st_wk_lock);
	return err;
}

/* wl18xx, wl128x BT, FM, GPS connectivity chip */
static struct ti_st_plat_data wilink_pdata = {
	.dev_name = WILINK_UART_DEV_NAME,
	.nshutdown_gpio = OMAP5_BT_NSHUTDOWN_GPIO, /* BT GPIO in OMAP5 */
	.flow_cntrl = 1,
	.baud_rate = 3686400, /* 115200 for test */
	.suspend = plat_wlink_kim_suspend,
	.resume = plat_wlink_kim_resume,
	.chip_enable = plat_uart_enable,
	.chip_disable = plat_uart_disable,
	.chip_asleep = plat_uart_disable,
	.chip_awake = plat_uart_enable,
};

static struct platform_device wl18xx_device = {
	.name           = "kim",
	.id             = -1,
	.dev.platform_data = &wilink_pdata,
};

static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

static struct platform_device nfcwilink_device = {
	.name = "nfcwilink",
	.id = -1,
};

static struct platform_device *omap5evm_wilink_devs[] = {
	&wl18xx_device,
	&btwilink_device,
	&nfcwilink_device,
};

static void __init omap5_ti_st_init(void)
{
	omap_mux_init_gpio(OMAP5_BT_NSHUTDOWN_GPIO,
		OMAP_PIN_OUTPUT | OMAP_PIN_INPUT_PULLUP);
	wake_lock_init(&st_wk_lock, WAKE_LOCK_SUSPEND, "st_wake_lock");

	platform_add_devices(omap5evm_wilink_devs,
				ARRAY_SIZE(omap5evm_wilink_devs));
}


int __init omap5_connectivity_init(void)
{
	omap5_evm_wifi_init();
#ifdef CONFIG_TI_ST
	/* add shared transport relevant platform devices only */
	omap5_ti_st_init();
#endif

	return 0;
}
