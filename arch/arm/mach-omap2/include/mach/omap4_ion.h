/*
 * arch/arm/mach-omap2/board-blaze.h
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _OMAP4_ION_H
#define _OMAP4_ION_H

#include <linux/ion.h>

#define OMAP4_RAM_SIZE (omap_total_ram_size())
#define PHYS_ADDR_SMC_MEM  (omap_smc_addr())
#define PHYS_ADDR_ION_HEAP_SECURE_INPUT_MEM  (omap_ion_heap_secure_input_addr())
#define PHYS_ADDR_ION_HEAP_SECURE_OUTPUT_WFDHDCP_MEM  \
			(omap_ion_heap_secure_output_wfdhdcp_addr())
#define PHYS_ADDR_ION_HEAP_TILER_MEM	(omap_ion_heap_tiler_mem_addr())
#define PHYS_ADDR_ION_HEAP_NONSECURE_TILER_MEM	\
			(omap_ion_heap_nonsec_tiler_mem_addr())

#define PHYS_ADDR_SMC_SIZE (omap_smc_size())
#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE (omap_ion_heap_secure_input_size())
#define OMAP4_ION_HEAP_SECURE_OUTPUT_WFDHDCP_SIZE \
			(omap_ion_heap_secure_output_wfdhdcp_size())
#define OMAP4_ION_HEAP_TILER_SIZE (omap_ion_heap_tiler_mem_size())
#define OMAP4_ION_HEAP_NONSECURE_TILER_SIZE	\
			(omap_ion_heap_nonsec_tiler_mem_size())


struct omap_ion_platform_data {
	struct ion_platform_data *ion;
	u32 tiler2d_size;
	u32 nonsecure_tiler2d_size;
};

struct omap_ion_platform_data *get_omap_ion_platform_data(void);

extern struct meminfo meminfo;

phys_addr_t omap_smc_addr(void);

size_t omap_smc_size(void);

#ifdef CONFIG_ION_OMAP
void omap4_ion_init(void);
void omap4_register_ion(void);

phys_addr_t omap_ion_heap_secure_input_addr(void);
phys_addr_t omap_ion_heap_secure_output_wfdhdcp_addr(void);
phys_addr_t omap_ion_heap_tiler_mem_addr(void);
phys_addr_t omap_ion_heap_nonsec_tiler_mem_addr(void);

size_t omap_ion_heap_secure_input_size(void);
size_t omap_ion_heap_secure_output_wfdhdcp_size(void);
size_t omap_ion_heap_tiler_mem_size(void);
size_t omap_ion_heap_nonsec_tiler_mem_size(void);

#else
static inline void omap4_ion_init(void) { return; }
static inline void omap4_register_ion(void) { return; }

phys_addr_t omap_ion_heap_secure_input_addr(void) { return 0; }
phys_addr_t omap_ion_heap_secure_output_wfdhdcp_addr(void) { return 0; }
phys_addr_t omap_ion_heap_tiler_mem_addr(void) { return 0; }
phys_addr_t omap_ion_heap_nonsec_tiler_mem_addr(void) { return 0; }

size_t omap_ion_heap_secure_input_size(void) { return 0; }
size_t omap_ion_heap_secure_output_wfdhdcp_size(void) { return 0; }
size_t omap_ion_heap_tiler_mem_size(void) { return 0; }
size_t omap_ion_heap_nonsec_tiler_mem_size(void) { return 0; }
#endif

#endif

