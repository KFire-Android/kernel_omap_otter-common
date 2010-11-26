/*
 * Copyright (C) 2009 Texas Instruments Inc.
 *
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

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include <mach/board-zoom.h>

#include <plat/common.h>
#include <plat/board.h>
#include <plat/usb.h>

#include "mux.h"
#include "sdram-hynix-h8mbx00u0mer-0em.h"
#include "smartreflex-class3.h"

static void __init omap_zoom_map_io(void)
{
	omap2_set_globals_36xx();
	omap34xx_map_common_io();
}

static struct omap_board_config_kernel zoom_config[] __initdata = {
};

/*
 * Note! The addresses which appear in this table must agree with the
 * addresses in the U-Boot environment variables.
 */
static struct mtd_partition zoom_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name           = "X-Loader-NAND",
		.offset         = 0,
		.size           = 4 * (64 * 2048),
		.mask_flags     = MTD_WRITEABLE,        /* force read-only */
	},
	{
		.name           = "U-Boot-NAND",
		.offset         = 0x0080000,
		.size           = 0x0180000, /* 1.5 M */
		.mask_flags     = MTD_WRITEABLE,        /* force read-only */
	},
	{
		.name           = "Boot Env-NAND",
		.offset         = 0x01C0000,
		.size           = 0x0040000,
	},
	{
		.name           = "Kernel-NAND",
		.offset         = 0x0200000,
		.size           = 0x1C00000,   /* 30M */
	},
#ifdef CONFIG_ANDROID
	{
		.name           = "system",
		.offset         = 0x2000000,
		.size           = 0xB400000,   /* 180M */
	},
	{
		.name           = "userdata",
		.offset         = 0xD400000,
		.size           = 0x4000000,    /* 64M */
	},
	{
		.name           = "cache",
		.offset         = 0x11400000,
		.size           = 0x2000000,    /* 32M */
	},
#endif
};

static struct flash_partitions zoom_flash_partitions[] = {
	{
		.parts = zoom_nand_partitions,
		.nr_parts = ARRAY_SIZE(zoom_nand_partitions),
	},
};

static void __init omap_zoom_init_irq(void)
{
	omap_board_config = zoom_config;
	omap_board_config_size = ARRAY_SIZE(zoom_config);
	omap2_init_common_hw(h8mbx00u0mer0em_sdrc_params,
			h8mbx00u0mer0em_sdrc_params);
	omap_init_irq();
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif

static const struct usbhs_omap_platform_data usbhs_pdata __initconst = {
	.port_mode[0]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[1]		= OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[2]		= OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset		= true,
	.reset_gpio_port[0]	= -EINVAL,
	.reset_gpio_port[1]	= 64,
	.reset_gpio_port[2]	= -EINVAL,
};

static void __init omap_zoom_init(void)
{
	omap3_mux_init(board_mux, OMAP_PACKAGE_CBP);
	zoom_peripherals_init();
	zoom_flash_init(zoom_flash_partitions, ZOOM_NAND_CS);
	zoom_debugboard_init();
	zoom_display_init(OMAP_DSS_VENC_TYPE_COMPOSITE);

	omap_mux_init_gpio(64, OMAP_PIN_OUTPUT);
	usb_uhhtll_init(&usbhs_pdata);
	usb_ehci_init();
	sr_class3_init();
}

MACHINE_START(OMAP_ZOOM3, "OMAP Zoom3 board")
	.phys_io	= ZOOM_UART_BASE,
	.io_pg_offst	= (ZOOM_UART_VIRT >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap_zoom_map_io,
	.init_irq	= omap_zoom_init_irq,
	.init_machine	= omap_zoom_init,
	.timer		= &omap_timer,
MACHINE_END
