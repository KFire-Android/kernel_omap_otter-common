/*
 * Copyright (C) 2009 Texas Instruments Inc.
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
#include <linux/mtd/nand.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/usb/phy.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "common.h"
#include "gpmc-smc91x.h"

#include "board-zoom.h"

#include "board-flash.h"
#include "mux.h"
#include "sdram-hynix-h8mbx00u0mer-0em.h"

#if defined(CONFIG_SMC91X) || defined(CONFIG_SMC91X_MODULE)

static struct omap_smc91x_platform_data board_smc91x_data = {
	.cs             = 3,
	.flags          = GPMC_MUX_ADD_DATA | IORESOURCE_IRQ_LOWLEVEL,
};

static void __init board_smc91x_init(void)
{
	board_smc91x_data.gpio_irq = 158;
	gpmc_smc91x_init(&board_smc91x_data);
}

#else

static inline void board_smc91x_init(void)
{
}

#endif /* defined(CONFIG_SMC91X) || defined(CONFIG_SMC91X_MODULE) */

static void enable_board_wakeup_source(void)
{
	/* T2 interrupt line (keypad) */
	omap_mux_init_signal("sys_nirq",
		OMAP_WAKEUP_EN | OMAP_PIN_INPUT_PULLUP);
}

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
	.gpio = 126,
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
	.gpio = 61,
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

static struct usbhs_omap_platform_data usbhs_bdata __initdata = {

	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_EHCI_PORT_MODE_PHY,
};

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#endif

/*
 * SDP3630 CS organization
 * See also the Switch S8 settings in the comments.
 */
static char chip_sel_sdp[][GPMC_CS_NUM] = {
	{PDC_NOR, PDC_NAND, PDC_ONENAND, DBG_MPDB, 0, 0, 0, 0}, /* S8:1111 */
	{PDC_ONENAND, PDC_NAND, PDC_NOR, DBG_MPDB, 0, 0, 0, 0}, /* S8:1110 */
	{PDC_NAND, PDC_ONENAND, PDC_NOR, DBG_MPDB, 0, 0, 0, 0}, /* S8:1101 */
};

static struct mtd_partition sdp_nor_partitions[] = {
	/* bootloader (U-Boot, etc) in first sector */
	{
		.name		= "Bootloader-NOR",
		.offset		= 0,
		.size		= SZ_256K,
		.mask_flags	= MTD_WRITEABLE, /* force read-only */
	},
	/* bootloader params in the next sector */
	{
		.name		= "Params-NOR",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_256K,
		.mask_flags	= 0,
	},
	/* kernel */
	{
		.name		= "Kernel-NOR",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_2M,
		.mask_flags	= 0
	},
	/* file system */
	{
		.name		= "Filesystem-NOR",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= 0
	}
};

static struct mtd_partition sdp_onenand_partitions[] = {
	{
		.name		= "X-Loader-OneNAND",
		.offset		= 0,
		.size		= 4 * (64 * 2048),
		.mask_flags	= MTD_WRITEABLE  /* force read-only */
	},
	{
		.name		= "U-Boot-OneNAND",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 2 * (64 * 2048),
		.mask_flags	= MTD_WRITEABLE  /* force read-only */
	},
	{
		.name		= "U-Boot Environment-OneNAND",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 1 * (64 * 2048),
	},
	{
		.name		= "Kernel-OneNAND",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 16 * (64 * 2048),
	},
	{
		.name		= "File System-OneNAND",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct mtd_partition sdp_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "X-Loader-NAND",
		.offset		= 0,
		.size		= 4 * (64 * 2048),
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot-NAND",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x80000 */
		.size		= 10 * (64 * 2048),
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "Boot Env-NAND",

		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x1c0000 */
		.size		= 6 * (64 * 2048),
	},
	{
		.name		= "Kernel-NAND",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x280000 */
		.size		= 40 * (64 * 2048),
	},
	{
		.name		= "File System - NAND",
		.size		= MTDPART_SIZ_FULL,
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x780000 */
	},
};

static struct flash_partitions sdp_flash_partitions[] = {
	{
		.parts = sdp_nor_partitions,
		.nr_parts = ARRAY_SIZE(sdp_nor_partitions),
	},
	{
		.parts = sdp_onenand_partitions,
		.nr_parts = ARRAY_SIZE(sdp_onenand_partitions),
	},
	{
		.parts = sdp_nand_partitions,
		.nr_parts = ARRAY_SIZE(sdp_nand_partitions),
	},
};

static struct platform_device *sdp3630_devices[] __initdata = {
	&hsusb1_phy_device,
	&hsusb1_reset_device,
	&hsusb2_phy_device,
	&hsusb2_reset_device,
};

static void __init omap_sdp_init(void)
{
	omap3_mux_init(board_mux, OMAP_PACKAGE_CBP);
	zoom_peripherals_init();
	omap_sdrc_init(h8mbx00u0mer0em_sdrc_params,
				  h8mbx00u0mer0em_sdrc_params);
	zoom_display_init();
	board_smc91x_init();
	board_flash_init(sdp_flash_partitions, chip_sel_sdp, NAND_BUSWIDTH_16);
	enable_board_wakeup_source();

	platform_add_devices(sdp3630_devices, ARRAY_SIZE(sdp3630_devices));

	/* PHY on HSUSB Port 1 i.e. index 0 */
	usb_bind_phy("ehci-omap.0", 0, "nop_usb_xceiv.1");
	/* PHY on HSUSB Port 2 i.e. index 1 */
	usb_bind_phy("ehci-omap.0", 1, "nop_usb_xceiv.2");

	usbhs_init(&usbhs_bdata);
}

MACHINE_START(OMAP_3630SDP, "OMAP 3630SDP board")
	.atag_offset	= 0x100,
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= omap3630_init_early,
	.init_irq	= omap3_init_irq,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_sdp_init,
	.init_late	= omap3630_init_late,
	.timer		= &omap3_timer,
	.restart	= omap3xxx_restart,
MACHINE_END
