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

#include <mach/omap4_ion.h>

static struct ion_platform_data omap4_ion_data = {
	.nr = 3,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_INPUT,
			.name = "secure_input",
			.base = PHYS_ADDR_SMC_MEM,
			.size = -1,
		},
		{	.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_TILER,
			.name = "tiler",
			.base = PHYS_ADDR_DUCATI_MEM,
			.size = -1,
		},
		{
			.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_NONSECURE_TILER,
			.name = "nonsecure_tiler",
			.base = 0,	/* append before prior */
			.size = -1,
		},
	},
};

static struct omap_ion_platform_data omap4_ion_pdata = {
	.ion = &omap4_ion_data,
	.tiler2d_size = OMAP4_ION_HEAP_TILER_SIZE,
	.nonsecure_tiler2d_size = OMAP4_ION_HEAP_NONSECURE_TILER_SIZE,
};

static struct platform_device omap4_ion_device = {
	.name = "ion-omap4",
	.id = -1,
	.dev = {
		.platform_data = &omap4_ion_data,
	},
};

struct omap_ion_platform_data *get_omap_ion_platform_data(void)
{
	return &omap4_ion_pdata;
}

void __init omap4_register_ion(void)
{
	platform_device_register(&omap4_ion_device);
}

void __init omap_ion_init(void)
{
	int i;
	int ret;
	u32 nonsecure = omap4_ion_pdata.nonsecure_tiler2d_size;

	for (i = 0; i < omap4_ion_data.nr; i++) {
		struct ion_platform_heap *h = &omap4_ion_data.heaps[i];
		bool backward = 0 > (s32) h->size;

		if (backward)
			h->size = -h->size;
		if (h->base == 0)
			/* continue after/before previous heap */
			h->base = h[-1].base + (backward ? 0 : h[-1].size);

		switch (h->id) {
		case OMAP_ION_HEAP_SECURE_INPUT:
			h->size = OMAP4_ION_HEAP_SECURE_INPUT_SIZE;
			break;
		case OMAP_ION_HEAP_NONSECURE_TILER:
			h->size = nonsecure;
			break;
		case OMAP_ION_HEAP_TILER:
			/* total TILER carveouts must be aligned to 2M */
			h->size = ALIGN(omap4_ion_pdata.tiler2d_size +
					nonsecure, SZ_2M) - nonsecure;
			break;
		default:
			break;
		}

		if (backward)
			h->base -= h->size;
		pr_info("%s: id=%u [%lx-%lx] size=%x\n", __func__, h->id,
					h->base, h->base + h->size, h->size);
	}

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
