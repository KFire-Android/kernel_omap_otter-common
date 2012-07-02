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

#ifndef CONFIG_ION_OMAP_DYNAMIC

#define OMAP4_RAM_SIZE (omap_total_ram_size())
#define PHYS_ADDR_SMC_MEM  (omap_smc_addr())
#define PHYS_ADDR_ION_HEAP_SECURE_INPUT_MEM  (omap_ion_heap_secure_input_addr())
#define PHYS_ADDR_ION_HEAP_SECURE_OUTPUT_WFDHDCP_MEM  \
			(omap_ion_heap_secure_output_wfdhdcp_addr())
#define PHYS_ADDR_DUCATI_MEM	(omap_ducati_heap_addr())
#define PHYS_ADDR_ION_HEAP_TILER_MEM	(omap_ion_heap_tiler_mem_addr())
#define PHYS_ADDR_ION_HEAP_NONSECURE_TILER_MEM	\
			(omap_ion_heap_nonsec_tiler_mem_addr())

#define PHYS_ADDR_SMC_SIZE (omap_smc_size())
#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE (omap_ion_heap_secure_input_size())
#define OMAP4_ION_HEAP_SECURE_OUTPUT_WFDHDCP_SIZE \
			(omap_ion_heap_secure_output_wfdhdcp_size())
#define PHYS_ADDR_DUCATI_SIZE (omap_ducati_heap_size())
#define OMAP4_ION_HEAP_TILER_SIZE (omap_ion_heap_tiler_mem_size())
#define OMAP4_ION_HEAP_NONSECURE_TILER_SIZE	\
			(omap_ion_heap_nonsec_tiler_mem_size())

#else

#define PHYS_ADDR_DUCATI_SIZE			(SZ_1M * 55)
#define PHYS_ADDR_DUCATI_MEM			(0x80000000 + SZ_512M - PHYS_ADDR_DUCATI_SIZE)
#define PHYS_ADDR_SMC_SIZE			(SZ_1M * 3)
#define PHYS_ADDR_SMC_MEM			(PHYS_ADDR_DUCATI_MEM - PHYS_ADDR_SMC_SIZE)
// #ifdef CONFIG_OMAP_REMOTE_PROC_DSP
#define PHYS_ADDR_TESLA_SIZE			(SZ_1M * 4)
#define PHYS_ADDR_TESLA_MEM			(PHYS_ADDR_SMC_MEM - PHYS_ADDR_TESLA_SIZE)
// #endif
#define OMAP4_ION_HEAP_TILER_SIZE		(SZ_512K)
#define PHYS_ADDR_ION_HEAP_TILER		(PHYS_ADDR_TESLA_MEM - OMAP4_ION_HEAP_TILER_SIZE)
#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE	(SZ_512K)
#define PHYS_ADDR_ION_HEAP_SECURE_INPUT		(PHYS_ADDR_ION_HEAP_TILER - OMAP4_ION_HEAP_SECURE_INPUT_SIZE)
#define OMAP_RAM_CONSOLE_SIZE 			(SZ_1M)
#define PHYS_ADDR_OMAP_RAM_CONSOLE		(PHYS_ADDR_ION_HEAP_SECURE_INPUT - OMAP_RAM_CONSOLE_SIZE)

#endif

struct omap_ion_platform_data {
	struct ion_platform_data *ion;
	u32 tiler2d_size;
	u32 nonsecure_tiler2d_size;
};

struct omap_ion_platform_data *get_omap_ion_platform_data(void);

extern struct meminfo meminfo;

phys_addr_t omap_smc_addr(void);
phys_addr_t omap_ducati_heap_addr(void);

size_t omap_smc_size(void);
size_t omap_ducati_heap_size(void);

#ifdef CONFIG_ION_OMAP
void omap_ion_init(void);
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
static inline void omap_ion_init(void) { return; }
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
