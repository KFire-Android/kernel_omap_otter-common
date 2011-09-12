/*
 * ION Initialization for OMAP4.
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/ion.h>
#include <linux/memblock.h>
#include <linux/omap_ion.h>
#include <linux/platform_device.h>

#include <plat/common.h>

#include "board-blaze.h"

#define OMAP4_RAMCONSOLE_START	(PLAT_PHYS_OFFSET + SZ_512M)
#define OMAP4_RAMCONSOLE_SIZE	SZ_2M
#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE	(SZ_1M * 30)
#define OMAP4_ION_HEAP_TILER_SIZE		SZ_128M
#define OMAP4_ION_HEAP_LARGE_SURFACES_SIZE	SZ_32M

#define PHYS_ADDR_SMC_SIZE	(SZ_1M * 3)
#define PHYS_ADDR_SMC_MEM	(0x80000000 + SZ_1G - PHYS_ADDR_SMC_SIZE)
#define PHYS_ADDR_DUCATI_SIZE	(SZ_1M * 105)
#define PHYS_ADDR_DUCATI_MEM	(PHYS_ADDR_SMC_MEM - PHYS_ADDR_DUCATI_SIZE - \
				OMAP4_ION_HEAP_SECURE_INPUT_SIZE)

static struct ion_platform_data omap4_ion_data = {
	.nr = 3,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_INPUT,
			.name = "secure_input",
			.base = PHYS_ADDR_SMC_MEM -
					OMAP4_ION_HEAP_SECURE_INPUT_SIZE,
			.size = OMAP4_ION_HEAP_SECURE_INPUT_SIZE,
		},
		{	.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_TILER,
			.name = "tiler",
			.base = PHYS_ADDR_DUCATI_MEM -
					OMAP4_ION_HEAP_TILER_SIZE,
			.size = OMAP4_ION_HEAP_TILER_SIZE,
		},
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_LARGE_SURFACES,
			.name = "large_surfaces",
			.base = 0x80000000 + SZ_512M + OMAP4_RAMCONSOLE_SIZE,
			.size = OMAP4_ION_HEAP_LARGE_SURFACES_SIZE,
		},
	},
};

static struct platform_device omap4_ion_device = {
	.name = "ion-omap4",
	.id = -1,
	.dev = {
		.platform_data = &omap4_ion_data,
	},
};

void __init omap4_register_ion(void)
{
	platform_device_register(&omap4_ion_device);
}

void __init omap_ion_init(void)
{
	int i;
	int ret;

	omap_reserve();
	memblock_remove(OMAP4_RAMCONSOLE_START, OMAP4_RAMCONSOLE_SIZE);

	for (i = 0; i < omap4_ion_data.nr; i++)
		if (omap4_ion_data.heaps[i].type == ION_HEAP_TYPE_CARVEOUT ||
		    omap4_ion_data.heaps[i].type == OMAP_ION_HEAP_TYPE_TILER) {
			ret = memblock_remove(omap4_ion_data.heaps[i].base,
					      omap4_ion_data.heaps[i].size);
			if (ret)
				pr_err("memblock remove of %x@%lx failed\n",
				       omap4_ion_data.heaps[i].size,
				       omap4_ion_data.heaps[i].base);
		}

}
