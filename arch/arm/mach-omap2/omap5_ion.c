/*
 * ION Initialization for OMAP5.
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

#include "omap5_ion.h"

static struct ion_platform_data omap5_ion_data = {
	.nr = 5,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_INPUT,
			.name = "secure_input",
			.base = PHYS_ADDR_SMC_MEM -
					OMAP_ION_HEAP_SECURE_INPUT_SIZE,
			.size = OMAP_ION_HEAP_SECURE_INPUT_SIZE,
		},
		{	.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_TILER,
			.name = "tiler",
			.base = PHYS_ADDR_SMC_MEM -
					OMAP_ION_HEAP_SECURE_INPUT_SIZE -
					OMAP5_ION_HEAP_TILER_SIZE,
			.size = OMAP5_ION_HEAP_TILER_SIZE,
		},
		{
			.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_NONSECURE_TILER,
			.name = "nonsecure_tiler",
			.base = PHYS_ADDR_SMC_MEM -
					OMAP_ION_HEAP_SECURE_INPUT_SIZE -
					OMAP5_ION_HEAP_TILER_SIZE -
					OMAP5_ION_HEAP_NONSECURE_TILER_SIZE,
			.size = OMAP5_ION_HEAP_NONSECURE_TILER_SIZE,
		},
		{
			.type = ION_HEAP_TYPE_SYSTEM,
			.id = OMAP_ION_HEAP_SYSTEM,
			.name = "system",
		},
		{
			.type = OMAP_ION_HEAP_TYPE_TILER_RESERVATION,
			.id = OMAP_ION_HEAP_TILER_RESERVATION,
			.name = "tiler_reservation",
		},
	},
};

static struct platform_device omap5_ion_device = {
	.name = "ion-omap",
	.id = -1,
	.dev = {
		.platform_data = &omap5_ion_data,
	},
};

void __init omap5_register_ion(void)
{
	platform_device_register(&omap5_ion_device);
}

void __init omap5_ion_init(void)
{
	int i;
	int ret;

	for (i = 0; i < omap5_ion_data.nr; i++)
		if (omap5_ion_data.heaps[i].type == ION_HEAP_TYPE_CARVEOUT ||
		    omap5_ion_data.heaps[i].type == OMAP_ION_HEAP_TYPE_TILER) {
			ret = memblock_remove(omap5_ion_data.heaps[i].base,
					      omap5_ion_data.heaps[i].size);
			if (ret)
				pr_err("memblock remove of %x@%lx failed\n",
				       omap5_ion_data.heaps[i].size,
				       omap5_ion_data.heaps[i].base);
		}

}
