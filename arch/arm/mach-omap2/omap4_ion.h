/*
 * arch/arm/mach-omap2/omap4_ion.h
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

#define OMAP4_ION_HEAP_SECURE_INPUT_SIZE	(SZ_1M * 90)
#define OMAP4_ION_HEAP_TILER_SIZE		(SZ_128M - SZ_32M)
#define OMAP4_ION_HEAP_NONSECURE_TILER_SIZE	SZ_32M

#define PHYS_ADDR_SMC_SIZE	(SZ_1M * 3)
#define PHYS_ADDR_SMC_MEM	(0x80000000 + SZ_1G - PHYS_ADDR_SMC_SIZE)

#ifdef CONFIG_ION_OMAP
void omap4_ion_init(void);
void omap4_register_ion(void);
#else
static inline void omap4_ion_init(void) { return; }
static inline void omap4_register_ion(void) { return; }
#endif

#endif
