/*
 * Support for AM3517/05 Craneboard
 * http://www.mistralsolutions.com/products/craneboard.php
 *
 * Copyright (C) 2010 Mistral Solutions Pvt Ltd. <www.mistralsolutions.com>
 * Author: R.Srinath <srinath@mistralsolutions.com>
 *
 * Based on mach-omap2/board-am3517evm.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as  published by the
 * Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/usb/phy.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include "common.h"

#include "am35xx-emac.h"
#include "mux.h"
#include "control.h"

#define GPIO_USB_POWER		35
#define GPIO_USB_NRESET		38

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#endif

/* PHY device on HS USB Port 1 i.e. nop_usb_xceiv.1 */
static struct platform_device hsusb1_phy_device = {
	.name = "nop_usb_xceiv",
	.id = 1,
};

/* Regulator for HS USB Port 1 PHY reset */
static struct regulator_consumer_supply hsusb1_reset_supplies[] = {
	/* Link PHY device to reset supply so it gets used in the PHY driver */
	REGULATOR_SUPPLY("reset", "nop_usb_xceiv.1"),
};

static struct regulator_init_data hsusb1_reset_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.consumer_supplies = hsusb1_reset_supplies,
	.num_consumer_supplies = ARRAY_SIZE(hsusb1_reset_supplies),
};

static struct fixed_voltage_config hsusb1_reset_config = {
	.supply_name = "hsusb1_reset",
	.microvolts = 3300000,
	.gpio = GPIO_USB_NRESET,
	.startup_delay = 70000, /* 70msec */
	.enable_high = 1,
	.enabled_at_boot = 0,   /* keep in RESET */
	.init_data = &hsusb1_reset_data,
};

static struct platform_device hsusb1_reset_device = {
	.name = "reg-fixed-voltage",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data = &hsusb1_reset_config,
	},
};

/* Regulator for HS USB Port 1 supply */
static struct regulator_consumer_supply hsusb1_power_supplies[] = {
/* Link PHY device to power supply so it gets enabled in the PHY driver */
	REGULATOR_SUPPLY("vcc", "nop_usb_xceiv.1"),
};

static struct regulator_init_data hsusb1_power_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.consumer_supplies = hsusb1_power_supplies,
	.num_consumer_supplies  = ARRAY_SIZE(hsusb1_power_supplies),
};

static struct fixed_voltage_config hsusb1_power_config = {
	.supply_name = "hsusb1_vbus",
	.microvolts = 5000000,
	.gpio = GPIO_USB_POWER,
	.startup_delay = 70000, /* 70msec */
	.enable_high = 1,
	.enabled_at_boot = 0,
	.init_data = &hsusb1_power_data,
};

static struct platform_device hsusb1_power_device = {
	.name = "reg-fixed-voltage",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data = &hsusb1_power_config,
	},
};

static struct usbhs_omap_platform_data usbhs_bdata __initdata = {
	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
};

static struct platform_device *am3517_crane_devices[] __initdata = {
	&hsusb1_phy_device,
	&hsusb1_reset_device,
	&hsusb1_power_device,
};

static void __init am3517_crane_init(void)
{
	int ret;

	omap3_mux_init(board_mux, OMAP_PACKAGE_CBB);
	omap_serial_init();
	omap_sdrc_init(NULL, NULL);

	/* Configure GPIO for EHCI port */
	if (omap_mux_init_gpio(GPIO_USB_NRESET, OMAP_PIN_OUTPUT)) {
		pr_err("Can not configure mux for GPIO_USB_NRESET %d\n",
			GPIO_USB_NRESET);
		return;
	}

	if (omap_mux_init_gpio(GPIO_USB_POWER, OMAP_PIN_OUTPUT)) {
		pr_err("Can not configure mux for GPIO_USB_POWER %d\n",
			GPIO_USB_POWER);
		return;
	}

	platform_add_devices(am3517_crane_devices,
				ARRAY_SIZE(am3517_crane_devices));

	/* PHY on HSUSB Port 1 i.e. index 0 */
	usb_bind_phy("ehci-omap.0", 0, "nop_usb_xceiv.1");

	usbhs_init(&usbhs_bdata);
	am35xx_emac_init(AM35XX_DEFAULT_MDIO_FREQUENCY, 1);
}

MACHINE_START(CRANEBOARD, "AM3517/05 CRANEBOARD")
	.atag_offset	= 0x100,
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= am35xx_init_early,
	.init_irq	= omap3_init_irq,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= am3517_crane_init,
	.init_late	= am35xx_init_late,
	.timer		= &omap3_timer,
	.restart	= omap3xxx_restart,
MACHINE_END
