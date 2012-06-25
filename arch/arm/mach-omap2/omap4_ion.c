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

#include <mach/omap4_ion.h>

/*
 * Carveouts from higher end of RAM
 *   - SMC
 *   - ION 1D
 *   - Ducati heap
 *   - Tiler 2D secure
 *   - Tiler non-secure
 */

static bool system_512m;

static phys_addr_t omap4_smc_addr;
static phys_addr_t omap4_ion_heap_secure_input_addr;
static phys_addr_t omap4_ion_heap_secure_output_wfdhdcp_addr;
static phys_addr_t omap4_ducati_heap_addr;
static phys_addr_t omap4_ion_heap_tiler_mem_addr;
static phys_addr_t omap4_ion_heap_nonsec_tiler_mem_addr;

static size_t omap4_smc_size;
static size_t omap4_ion_heap_secure_input_size;
static size_t omap4_ion_heap_secure_output_wfdhdcp_size;
static size_t omap4_ducati_heap_size;
static size_t omap4_ion_heap_tiler_mem_size;
static size_t omap4_ion_heap_nonsec_tiler_mem_size;

static struct ion_platform_data omap4_ion_data = {
	.nr = 6,
	.heaps = {
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_INPUT,
			.name = "secure_input",
		},
		{
			.type = ION_HEAP_TYPE_CARVEOUT,
			.id = OMAP_ION_HEAP_SECURE_OUTPUT_WFDHDCP,
			.name = "secure_output_wfdhdcp",
		},
		{	.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_TILER,
			.name = "tiler",
		},
		{
			.type = OMAP_ION_HEAP_TYPE_TILER,
			.id = OMAP_ION_HEAP_NONSECURE_TILER,
			.name = "nonsecure_tiler",
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

static struct omap_ion_platform_data omap4_ion_pdata = {
	.ion = &omap4_ion_data,
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

	system_512m = (omap_total_ram_size() == SZ_512M);

	/* carveout sizes */
	omap4_smc_size = (SZ_1M * 3);

	if (system_512m) {
		omap4_ion_heap_secure_input_size = 0;
		omap4_ion_heap_secure_output_wfdhdcp_size = 0;
		omap4_ducati_heap_size = (SZ_1M * 83);
		omap4_ion_heap_nonsec_tiler_mem_size = 0;
		omap4_ion_heap_tiler_mem_size = 0;
	} else {
		omap4_ion_heap_secure_input_size = (SZ_1M * 90);
		omap4_ion_heap_secure_output_wfdhdcp_size = (SZ_1M * 16);
		omap4_ducati_heap_size = (SZ_1M * 105);
		omap4_ion_heap_nonsec_tiler_mem_size = nonsecure;
		omap4_ion_heap_tiler_mem_size =
					 (ALIGN(omap4_ion_pdata.tiler2d_size +
					 nonsecure, SZ_2M) - nonsecure);
	}

	/* carveout addresses */
	omap4_smc_addr = PLAT_PHYS_OFFSET + omap_total_ram_size() -
				omap4_smc_size;
	omap4_ion_heap_secure_input_addr = omap4_smc_addr -
				omap4_ion_heap_secure_input_size;
	omap4_ion_heap_secure_output_wfdhdcp_addr =
				omap4_ion_heap_secure_input_addr -
				omap4_ion_heap_secure_output_wfdhdcp_size;
	omap4_ducati_heap_addr = omap4_ion_heap_secure_output_wfdhdcp_addr -
				omap4_ducati_heap_size;
	omap4_ion_heap_tiler_mem_addr = omap4_ducati_heap_addr -
				omap4_ion_heap_tiler_mem_size;
	omap4_ion_heap_nonsec_tiler_mem_addr = omap4_ion_heap_tiler_mem_addr -
				omap4_ion_heap_nonsec_tiler_mem_size;

	pr_info("omap4_total_ram_size = 0x%x\n" \
				"omap4_smc_size = 0x%x\n"  \
				"omap4_ion_heap_secure_input_size = 0x%x\n"  \
				"omap4_ion_heap_secure_output_wfdhdcp_size = 0x%x\n"  \
				"omap4_ducati_heap_size = 0x%x\n"  \
				"omap4_ion_heap_tiler_mem_size = 0x%x\n"  \
				"omap4_ion_heap_nonsec_tiler_mem_size  = 0x%x\n",
				omap_total_ram_size(),
				omap4_smc_size,
				omap4_ion_heap_secure_input_size,
				omap4_ion_heap_secure_output_wfdhdcp_size,
				omap4_ducati_heap_size,
				omap4_ion_heap_tiler_mem_size,
				omap4_ion_heap_nonsec_tiler_mem_size);

	pr_info(" omap4_smc_addr = 0x%x\n"  \
				"omap4_ion_heap_secure_input_addr = 0x%x\n"  \
				"omap4_ion_heap_secure_output_wfdhdcp_addr = 0x%x\n"  \
				"omap4_ducati_heap_addr = 0x%x\n"  \
				"omap4_ion_heap_tiler_mem_addr = 0x%x\n"  \
				"omap4_ion_heap_nonsec_tiler_mem_addr  = 0x%x\n",
				omap4_smc_addr,
				omap4_ion_heap_secure_input_addr,
				omap4_ion_heap_secure_output_wfdhdcp_addr,
				omap4_ducati_heap_addr,
				omap4_ion_heap_tiler_mem_addr,
				omap4_ion_heap_nonsec_tiler_mem_addr);

	for (i = 0; i < omap4_ion_data.nr; i++) {
		struct ion_platform_heap *h = &omap4_ion_data.heaps[i];

		switch (h->id) {
		case OMAP_ION_HEAP_SECURE_INPUT:
			h->base = omap4_ion_heap_secure_input_addr;
			h->size = omap4_ion_heap_secure_input_size;
			break;
		case OMAP_ION_HEAP_SECURE_OUTPUT_WFDHDCP:
			h->base = omap4_ion_heap_secure_output_wfdhdcp_addr;
			h->size = omap4_ion_heap_secure_output_wfdhdcp_size;
			break;
		case OMAP_ION_HEAP_NONSECURE_TILER:
			h->base = omap4_ion_heap_nonsec_tiler_mem_addr;
			h->size = omap4_ion_heap_nonsec_tiler_mem_size;
			break;
		case OMAP_ION_HEAP_TILER:
			h->base = omap4_ion_heap_tiler_mem_addr;
			h->size = omap4_ion_heap_tiler_mem_size;
			break;
		default:
			break;
		}
		pr_info("%s: %s id=%u [%lx-%lx] size=%x\n",
					__func__, h->name, h->id,
					h->base, h->base + h->size, h->size);
	}

	for (i = 0; i < omap4_ion_data.nr; i++)
		if (omap4_ion_data.heaps[i].type == ION_HEAP_TYPE_CARVEOUT ||
		    omap4_ion_data.heaps[i].type == OMAP_ION_HEAP_TYPE_TILER) {
			if (!omap4_ion_data.heaps[i].size)
				continue;
			ret = memblock_remove(omap4_ion_data.heaps[i].base,
					      omap4_ion_data.heaps[i].size);
			if (omap4_ion_data.heaps[i].id ==
					OMAP_ION_HEAP_SECURE_OUTPUT_WFDHDCP) {
				/* Reducing the actual size being mapped for Ion/Ducati as
				 * secure component uses the remaining memory */
				omap4_ion_data.heaps[i].size =
					omap4_ion_heap_secure_output_wfdhdcp_size >> 1;
			}
			if (ret)
				pr_err("memblock remove of %x@%lx failed\n",
				       omap4_ion_data.heaps[i].size,
				       omap4_ion_data.heaps[i].base);
		}
}

phys_addr_t omap_smc_addr(void)
{
	return omap4_smc_addr;
}

phys_addr_t omap_ion_heap_secure_input_addr(void)
{
	return omap4_ion_heap_secure_input_addr;
}

phys_addr_t omap_ion_heap_secure_output_wfdhdcp_addr(void)
{
	return omap4_ion_heap_secure_output_wfdhdcp_addr;
}

phys_addr_t omap_ducati_heap_addr(void)
{
	return omap4_ducati_heap_addr;
}

phys_addr_t omap_ion_heap_tiler_mem_addr(void)
{
	return omap4_ion_heap_tiler_mem_addr;
}

phys_addr_t omap_ion_heap_nonsec_tiler_mem_addr(void)
{
	return omap4_ion_heap_nonsec_tiler_mem_addr;
}

size_t omap_smc_size(void)
{
	return omap4_smc_size;
}

size_t omap_ion_heap_secure_input_size(void)
{
	return omap4_ion_heap_secure_input_size;
}

size_t omap_ion_heap_secure_output_wfdhdcp_size(void)
{
	return omap4_ion_heap_secure_output_wfdhdcp_size;
}

size_t omap_ducati_heap_size(void)
{
	return omap4_ducati_heap_size;
}

size_t omap_ion_heap_tiler_mem_size(void)
{
	return omap4_ion_heap_tiler_mem_size;
}

size_t omap_ion_heap_nonsec_tiler_mem_size(void)
{
	return omap4_ion_heap_nonsec_tiler_mem_size;
}
