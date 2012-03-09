/*
 * OMAP ram console support
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Huzefa Kankroliwala <huzefank@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/platform_data/ram_console.h>
#include <linux/memblock.h>
#include <plat/cpu.h>
#include "resetreason.h"
#include "omap_ram_console.h"

static struct resource ram_console_resources[] = {
	{
		.flags  = IORESOURCE_MEM,
		.start  = OMAP_RAM_CONSOLE_START_DEFAULT,
		.end    = OMAP_RAM_CONSOLE_START_DEFAULT +
			  OMAP_RAM_CONSOLE_SIZE_DEFAULT - 1,
	},
};

static struct ram_console_platform_data ram_console_pdata;

static struct platform_device ram_console_device = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ram_console_resources),
	.resource	= ram_console_resources,
	.dev		= {
	.platform_data	= &ram_console_pdata,
	},
};

static __initdata bool omap_ramconsole_inited;

/**
 * omap_ram_console_register() - device_initcall to register ramconsole device
 */
static int __init omap_ram_console_register(void)
{
	int ret;

	if (!omap_ramconsole_inited)
		return -ENODEV;

	ret = platform_device_register(&ram_console_device);
	if (ret) {
		pr_err("%s: unable to register ram console device:"
			"start=0x%08x, end=0x%08x, ret=%d\n",
			__func__, (u32)ram_console_resources[0].start,
			(u32)ram_console_resources[0].end, ret);
		memblock_add(ram_console_resources[0].start,
			(ram_console_resources[0].end -
			 ram_console_resources[0].start + 1));
	}

	return ret;
}
device_initcall(omap_ram_console_register);

/**
 * omap_ram_console_init() - setup the ram console device for OMAP
 * @phy_addr:	physical address of the start of ram console buffer
 * @size:	ram console buffer size
 *
 * Board files call this as part of the memory reservation routine
 * Depending on the choice of memory geometry and desired size,
 * provide appropriate parameters.
 *
 * IMPORTANT: board files need to ensure that the DDR configurations
 * enable self refresh mode for this to function properly.
 */

int __init omap_ram_console_init(phys_addr_t phy_addr, size_t size)
{
	int ret;

	/* Remove the ram console region from kernel's map */
	ret = memblock_remove(phy_addr, size);
	if (ret) {
		pr_err("%s: unable to remove memory for ram console:"
			"start=0x%08x, size=0x%08x, ret=%d\n",
			__func__, (u32)phy_addr, (u32)size, ret);
		return ret;
	}

	/* Update reset reason for platforms that have support for it */
	if (cpu_is_omap44xx())
		ram_console_pdata.bootinfo = omap4_get_resetreason();

	ram_console_resources[0].start = phy_addr;
	ram_console_resources[0].end = phy_addr + size - 1;

	/* flag for registration */
	omap_ramconsole_inited = true;

	return ret;
}
