/*
 * Copyright (C) 2009-2010 Texas Instruments Inc.
 * Mikkel Christensen <mlc@ti.com>
 * Felipe Balbi <balbi@ti.com>
 *
 * Modified from mach-omap2/board-ldp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/mtd/nand.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/usb/phy.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "common.h"

#include "board-zoom.h"

#include "board-flash.h"
#include "mux.h"
#include "sdram-micron-mt46h32m32lf-6.h"
#include "sdram-hynix-h8mbx00u0mer-0em.h"

#define ZOOM3_EHCI_RESET_GPIO		64

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	/* WLAN IRQ - GPIO 162 */
	OMAP3_MUX(MCBSP1_CLKX, OMAP_MUX_MODE4 | OMAP_PIN_INPUT),
	/* WLAN POWER ENABLE - GPIO 101 */
	OMAP3_MUX(CAM_D2, OMAP_MUX_MODE4 | OMAP_PIN_OUTPUT),
	/* WLAN SDIO: MMC3 CMD */
	OMAP3_MUX(MCSPI1_CS1, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC3 CLK */
	OMAP3_MUX(ETK_CLK, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC3 DAT[0-3] */
	OMAP3_MUX(ETK_D3, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D4, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D5, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	OMAP3_MUX(ETK_D6, OMAP_MUX_MODE2 | OMAP_PIN_INPUT_PULLUP),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#endif

static struct mtd_partition zoom_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "X-Loader-NAND",
		.offset		= 0,
		.size		= 4 * (64 * 2048),	/* 512KB, 0x80000 */
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot-NAND",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x80000 */
		.size		= 10 * (64 * 2048),	/* 1.25MB, 0x140000 */
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "Boot Env-NAND",
		.offset		= MTDPART_OFS_APPEND,   /* Offset = 0x1c0000 */
		.size		= 2 * (64 * 2048),	/* 256KB, 0x40000 */
	},
	{
		.name		= "Kernel-NAND",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x0200000*/
		.size		= 240 * (64 * 2048),	/* 30M, 0x1E00000 */
	},
	{
		.name		= "system",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x2000000 */
		.size		= 3328 * (64 * 2048),	/* 416M, 0x1A000000 */
	},
	{
		.name		= "userdata",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x1C000000*/
		.size		= 256 * (64 * 2048),	/* 32M, 0x2000000 */
	},
	{
		.name		= "cache",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x1E000000*/
		.size		= 256 * (64 * 2048),	/* 32M, 0x2000000 */
	},
};

/* PHY device on HS USB Port 2 i.e. nop_usb_xceiv.2 */
static struct platform_device hsusb2_phy_device = {
	.name = "nop_usb_xceiv",
	.id = 2,
};

/* Regulator for HS USB Port 2 PHY reset */
static struct regulator_consumer_supply hsusb2_reset_supplies[] = {
	/* Link PHY device to reset supply so it gets used in the PHY driver */
	REGULATOR_SUPPLY("reset", "nop_usb_xceiv.2"),
};

static struct regulator_init_data hsusb2_reset_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.consumer_supplies = hsusb2_reset_supplies,
	.num_consumer_supplies = ARRAY_SIZE(hsusb2_reset_supplies),
};

static struct fixed_voltage_config hsusb2_reset_config = {
	.supply_name = "hsusb2_reset",
	.microvolts = 3300000,
	.gpio = ZOOM3_EHCI_RESET_GPIO,
	.startup_delay = 70000, /* 70msec */
	.enable_high = 1,
	.enabled_at_boot = 0,   /* keep in RESET */
	.init_data = &hsusb2_reset_data,
};

static struct platform_device hsusb2_reset_device = {
	.name = "reg-fixed-voltage",
	.id = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data = &hsusb2_reset_config,
	},
};

static struct platform_device *zoom3_devices[] __initdata = {
	&hsusb2_phy_device,
	&hsusb2_reset_device,
};

static struct usbhs_omap_platform_data usbhs_bdata __initdata = {
	.port_mode[1]		= OMAP_EHCI_PORT_MODE_PHY,
};

static void __init omap_zoom_init(void)
{
	if (machine_is_omap_zoom2()) {
		omap3_mux_init(board_mux, OMAP_PACKAGE_CBB);
	} else if (machine_is_omap_zoom3()) {
		omap3_mux_init(board_mux, OMAP_PACKAGE_CBP);
		omap_mux_init_gpio(ZOOM3_EHCI_RESET_GPIO, OMAP_PIN_OUTPUT);

		platform_add_devices(zoom3_devices, ARRAY_SIZE(zoom3_devices));
		/* PHY on HSUSB Port 2 i.e. index 1 */
		usb_bind_phy("ehci-omap.0", 1, "nop_usb_xceiv.2");
		usbhs_init(&usbhs_bdata);
	}

	board_nand_init(zoom_nand_partitions,
			ARRAY_SIZE(zoom_nand_partitions), ZOOM_NAND_CS,
			NAND_BUSWIDTH_16, nand_default_timings);
	zoom_debugboard_init();
	zoom_peripherals_init();

	if (machine_is_omap_zoom2())
		omap_sdrc_init(mt46h32m32lf6_sdrc_params,
					  mt46h32m32lf6_sdrc_params);
	else if (machine_is_omap_zoom3())
		omap_sdrc_init(h8mbx00u0mer0em_sdrc_params,
					  h8mbx00u0mer0em_sdrc_params);

	zoom_display_init();
}

MACHINE_START(OMAP_ZOOM2, "OMAP Zoom2 board")
	.atag_offset	= 0x100,
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= omap3430_init_early,
	.init_irq	= omap3_init_irq,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_zoom_init,
	.init_late	= omap3430_init_late,
	.timer		= &omap3_timer,
	.restart	= omap3xxx_restart,
MACHINE_END

MACHINE_START(OMAP_ZOOM3, "OMAP Zoom3 board")
	.atag_offset	= 0x100,
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= omap3630_init_early,
	.init_irq	= omap3_init_irq,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_zoom_init,
	.init_late	= omap3630_init_late,
	.timer		= &omap3_timer,
	.restart	= omap3xxx_restart,
MACHINE_END
