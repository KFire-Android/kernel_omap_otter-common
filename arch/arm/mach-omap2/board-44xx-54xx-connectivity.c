/*
 * arch/arm/mach-omap2/board-44xx-54xx-connectivity.c
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
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/wl12xx.h>

/* for TI Shared Transport devices */
#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>
#include <plat/omap-serial.h>
#include <plat/omap_apps_brd_id.h>
#include <linux/wakelock.h>

#include "mux.h"
#include "board-54xx-sevm.h"

#define OMAP4_WILINK_UART_DEV_NAME	"/dev/ttyO1"
#define OMAP4_GPIO_WIFI_IRQ		53
#define OMAP4_GPIO_WIFI_PMENA		54
#define OMAP4_BT_NSHUTDOWN_GPIO		55

#define OMAP5_WILINK_UART_DEV_NAME	"/dev/ttyO4"
#define OMAP5_BT_NSHUTDOWN_GPIO		142
#define OMAP5_GPIO_WIFI_PMENA		140
#define OMAP5_GPIO_WIFI_SEVM_IRQ	9
#define OMAP5_GPIO_WIFI_PANDA5_IRQ	14
#define MAX_ST_DEVS			5

struct wilink_board_init_data {
	s16				wifi_gpio_pmena;
	u16				wifi_pmena_mux_flags;
	s16				wifi_gpio_irq;
	u16				wifi_irq_mux_flags;
	s16				bt_gpio_shutdown;
	u16				bt_shutdown_mux_flags;
	struct wl12xx_platform_data	*wlan_data;
	struct platform_device		*vwlan_device;
	struct platform_device		*st_devs[MAX_ST_DEVS];
	struct {
	const char	*muxname;
	int		val;
	}				mux_signals[];
};

static struct regulator_consumer_supply omap4_vmmc5_supply = {
	.supply         = "vmmc",
	.dev_name       = "omap_hsmmc.4",
};

static struct regulator_consumer_supply omap5_evm_vmmc3_supply = {
	.supply         = "vmmc",
	.dev_name       = "omap_hsmmc.2",
};

static struct regulator_init_data omap4plus_vmmc_regulator = {
		.constraints            = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = &omap5_evm_vmmc3_supply,
};

static struct fixed_voltage_config omap4plus_vwlan = {
	.supply_name            = "vwl1271",
	.microvolts             = 1800000, /* 1.8V */
	.startup_delay          = 70000, /* 70msec */
	.enable_high            = 1,
	.enabled_at_boot        = 0,
	.init_data              = &omap4plus_vmmc_regulator,
};

static struct platform_device omap4plus_vwlan_device = {
	.name           = "reg-fixed-voltage",
	.id             = 2,
	.dev = {
		.platform_data = &omap4plus_vwlan,
	},
};

static struct wl12xx_platform_data omap4plus_wlan_data __initdata = {
	.board_ref_clock    = WL12XX_REFCLOCK_26,
	.board_tcxo_clock   = WL12XX_TCXOCLOCK_26,
};

static void
__init omap4plus_wifi_mux_init(const struct wilink_board_init_data *data)
{
	int ret = 0;
	unsigned i = 0;
	struct fixed_voltage_config *fv_conf = data->vwlan_device->dev.platform_data;

	if (data->wifi_gpio_irq != -1)
		omap_mux_init_gpio(data->wifi_gpio_irq,
				data->wifi_irq_mux_flags);

	if (data->wifi_gpio_pmena != -1)
		omap_mux_init_gpio(data->wifi_gpio_pmena,
				data->wifi_pmena_mux_flags);

	if (fv_conf)
		fv_conf->gpio = data->wifi_gpio_pmena;

	for (i = 0; data->mux_signals[i].muxname && !ret;
		ret = omap_mux_init_signal(data->mux_signals[i].muxname,
			data->mux_signals[i].val), i++)
		;

	if (ret) {
		i--;
		pr_err("%s: Error during pad initialization: %s:%d",
				__func__,
				data->mux_signals[i].muxname,
				data->mux_signals[i].val);
	}
}

static void
__init omap4plus_wifi_init(const struct wilink_board_init_data *data)
{
	int ret;

	omap4plus_wifi_mux_init(data);

	ret = gpio_request_one(data->wifi_gpio_irq, GPIOF_IN, "wlan");
	if (ret) {
		printk(KERN_INFO "wlan: IRQ gpio request failure in board file\n");
		return;
	}

	data->wlan_data->irq = gpio_to_irq(data->wifi_gpio_irq);

	ret = wl12xx_set_platform_data(data->wlan_data);
	if (ret) {
		pr_err("Error setting wl12xx data\n");
		return;
	}

	ret = platform_device_register(data->vwlan_device);
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
static int plat_uart_disable(struct kim_data_s *kdata)
{
	int port_id = 0;
	int err = 0;
	if (uart_req) {
		if (!kdata) {
			pr_err("%s: NULL kim_data pointer\n", __func__);
			return -EINVAL;
		}
		err = sscanf(kdata->dev_name, "/dev/ttyO%d", &port_id);
		if (!err) {
			pr_err("%s: Wrong UART name: %s\n", __func__,
				kdata->dev_name);
			return -EINVAL;
		}
		err = omap_serial_ext_uart_disable(port_id);
		if (!err)
			uart_req = false;
	}
	wake_unlock(&st_wk_lock);
	return err;
}

/* Call the uart enable of serial driver */
static int plat_uart_enable(struct kim_data_s *kdata)
{
	int port_id = 0;
	int err = 0;
	if (!uart_req) {
		if (!kdata) {
			pr_err("%s: NULL kim_data pointer\n", __func__);
			return -EINVAL;
		}
		err = sscanf(kdata->dev_name, "/dev/ttyO%d", &port_id);
		if (!err) {
			pr_err("%s: Wrong UART name: %s\n", __func__,
				kdata->dev_name);
			return -EINVAL;
		}
		err = omap_serial_ext_uart_enable(port_id);
		if (!err)
			uart_req = true;
	}
	wake_lock(&st_wk_lock);
	return err;
}

/* wl18xx, wl128x BT, FM, GPS connectivity chip */
static struct ti_st_plat_data omap4plus_wilink_pdata = {
	.dev_name = OMAP5_WILINK_UART_DEV_NAME,
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
	.dev.platform_data = &omap4plus_wilink_pdata,
};

static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

static struct platform_device nfcwilink_device = {
	.name = "nfcwilink",
	.id = -1,
};

static void
__init omap4plus_ti_st_init(const struct wilink_board_init_data *data)
{
	unsigned i;
	int ret = 0;

	if (data->bt_gpio_shutdown != -1)
		omap_mux_init_gpio(data->bt_gpio_shutdown,
				data->bt_shutdown_mux_flags);

	wake_lock_init(&st_wk_lock, WAKE_LOCK_SUSPEND, "st_wake_lock");

	for (i = 0; data->st_devs[i] && !ret;
			ret = platform_device_register(data->st_devs[i++]))
		;

	if (ret)
		pr_err("%s: Error register ST device: %u -> %d\n", __func__,
				--i, ret);
}

static struct wilink_board_init_data __initdata omap4_wilink_init_data = {
	.wifi_gpio_irq = OMAP4_GPIO_WIFI_IRQ,
	.wifi_irq_mux_flags = OMAP_PIN_INPUT | OMAP_PIN_OFF_WAKEUPENABLE,
	.wifi_gpio_pmena = OMAP4_GPIO_WIFI_PMENA,
	.wifi_pmena_mux_flags = OMAP_PIN_OUTPUT,
	.bt_gpio_shutdown = OMAP4_BT_NSHUTDOWN_GPIO,
	.bt_shutdown_mux_flags = OMAP_PIN_OUTPUT,
	.wlan_data = &omap4plus_wlan_data,
	.vwlan_device = &omap4plus_vwlan_device,
	.st_devs = {
		&wl18xx_device,
		&btwilink_device,
		&nfcwilink_device,
		NULL, /* Terminator */
	},
	.mux_signals = {
		{
			"sdmmc5_cmd.sdmmc5_cmd",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"sdmmc5_clk.sdmmc5_clk",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"sdmmc5_dat0.sdmmc5_dat0",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"sdmmc5_dat1.sdmmc5_dat1",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"sdmmc5_dat2.sdmmc5_dat2",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"sdmmc5_dat3.sdmmc5_dat3",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			NULL, /* Terminator */
		},
	}
};

static struct wilink_board_init_data __initdata omap5_wilink_init_data = {
	.wifi_gpio_irq = OMAP5_GPIO_WIFI_SEVM_IRQ,
	.wifi_irq_mux_flags = OMAP_PIN_INPUT_PULLDOWN,
	.wifi_gpio_pmena = OMAP5_GPIO_WIFI_PMENA,
	.wifi_pmena_mux_flags = OMAP_PIN_OUTPUT | OMAP_PIN_INPUT_PULLUP,
	.bt_gpio_shutdown = OMAP5_BT_NSHUTDOWN_GPIO,
	.bt_shutdown_mux_flags = OMAP_PIN_OUTPUT | OMAP_PIN_INPUT_PULLUP,
	.wlan_data = &omap4plus_wlan_data,
	.vwlan_device = &omap4plus_vwlan_device,
	.st_devs = {
		&wl18xx_device,
		&btwilink_device,
		NULL, /* Terminator */
	},
	.mux_signals = {
		{
			"wlsdio_cmd.wlsdio_cmd",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"wlsdio_clk.wlsdio_clk",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"wlsdio_data0.wlsdio_data0",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"wlsdio_data1.wlsdio_data1",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"wlsdio_data2.wlsdio_data2",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			"wlsdio_data3.wlsdio_data3",
			OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
		},
		{
			NULL, /* Terminator */
		},
	}
};

int __init omap4plus_connectivity_init(void)
{
	struct wilink_board_init_data *idata;
	int board_class = omap_get_board_class();

	switch (board_class) {
	case BOARD_OMAP5_SEVM:
	case BOARD_OMAP5_SEVM2:
	case BOARD_OMAP5_SEVM3:
		idata = &omap5_wilink_init_data;
		break;
	case BOARD_OMAP5_UEVM:
	case BOARD_OMAP5_UEVM2:
		idata = &omap5_wilink_init_data;
		idata->wifi_gpio_irq = OMAP5_GPIO_WIFI_PANDA5_IRQ;
		break;
	case BOARD_OMAP4_BLAZE:
	case BOARD_OMAP4_TABLET1:
	case BOARD_OMAP4_TABLET2:
		idata = &omap4_wilink_init_data;
		omap4plus_wilink_pdata.nshutdown_gpio = OMAP4_BT_NSHUTDOWN_GPIO;
		omap4plus_vmmc_regulator.consumer_supplies = &omap4_vmmc5_supply;
		strcpy(&omap4plus_wilink_pdata.dev_name[0],
				OMAP4_WILINK_UART_DEV_NAME);
		break;
	default:
		pr_err("%s: Error: Unsupported board class: 0x%04x\n",
				__func__, board_class);
		return -EINVAL;
	}

	omap4plus_wifi_init(idata);
#ifdef CONFIG_TI_ST
	/* add shared transport relevant platform devices only */
	omap4plus_ti_st_init(idata);
#endif
	return 0;
}
