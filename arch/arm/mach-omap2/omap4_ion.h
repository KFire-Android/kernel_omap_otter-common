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

/*
 * Adjusted for 512M
 * |----------- 0   (0x800000000)
 * |-------- FREE (353M) - 24M VRAM
 * |-------- 353 ION NON SECURE (32M)
 * |-------- 385 (TESLA *OPTIONAL) (4M)
 * |-------- 389 ION HEAP (40M)
 * |-------- 429 DUCATI SIZE (40M)
 * |-------- 469 ION HEAP SECURE (40M)
 * |-------- 509 SMC MEM (3M)
 * |----------- 512
 */
#ifndef _OMAP4_ION_H
#define _OMAP4_ION_H

#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE	(SZ_1M * 40)
#define OMAP4_ION_HEAP_TILER_SIZE		((SZ_1M * 72) - SZ_32M)
#define OMAP4_ION_HEAP_NONSECURE_TILER_SIZE	SZ_32M

#define PHYS_ADDR_SMC_SIZE	(SZ_1M * 3)
#define PHYS_ADDR_SMC_MEM	(0x80000000 + SZ_512M - PHYS_ADDR_SMC_SIZE)
#define PHYS_ADDR_DUCATI_SIZE	(SZ_1M * 45)
#define PHYS_ADDR_DUCATI_MEM	(PHYS_ADDR_SMC_MEM - PHYS_ADDR_DUCATI_SIZE - \
				OMAP4_ION_HEAP_SECURE_INPUT_SIZE)

#ifdef CONFIG_OMAP_REMOTE_PROC_DSP
#define PHYS_ADDR_TESLA_SIZE	(SZ_1M * 4)
#define PHYS_ADDR_TESLA_MEM	(PHYS_ADDR_DUCATI_MEM - \
					OMAP4_ION_HEAP_TILER_SIZE - \
					PHYS_ADDR_TESLA_SIZE)
#endif

#ifdef CONFIG_ION_OMAP
void omap_ion_init(void);
void omap4_register_ion(void);
#else
static inline void omap_ion_init(void) { return; }
static inline void omap4_register_ion(void) { return; }
#endif

#endif
