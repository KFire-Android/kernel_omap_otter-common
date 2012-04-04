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

#include "omap4_ion.h"

#ifndef CONFIG_ION_OMAP_DYNAMIC
static struct ion_platform_data omap4_ion_data = {
	.nr = 3,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_INPUT,
			.name = "secure_input",
			.base = PHYS_ADDR_ION_HEAP_SECURE_INPUT,
			.size = OMAP4_ION_HEAP_SECURE_INPUT_SIZE,
		},
		{	.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_TILER,
			.name = "tiler",
			.base = PHYS_ADDR_ION_HEAP_TILER,
			.size = OMAP4_ION_HEAP_TILER_SIZE,
		},
		{
			.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_NONSECURE_TILER,
			.name = "nonsecure_tiler",
			.base = PHYS_ADDR_ION_HEAP_NONSECURE_TILER,
			.size = OMAP4_ION_HEAP_NONSECURE_TILER_SIZE,
		},
	},
};
#else
//We need structure for mempool
// So declare at least one heap of each kind with 1MB size
static struct ion_platform_data omap4_ion_data = {
	.nr = 2,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_INPUT,
			.name = "secure_input",
			.base = PHYS_ADDR_ION_HEAP_SECURE_INPUT,
			.size = OMAP4_ION_HEAP_SECURE_INPUT_SIZE,
		},
		{	.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_TILER,
			.name = "tiler",
			.base = PHYS_ADDR_ION_HEAP_TILER,
			.size = OMAP4_ION_HEAP_TILER_SIZE,
		},
	},
};
#endif

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
