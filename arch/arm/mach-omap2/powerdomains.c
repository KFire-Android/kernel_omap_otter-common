/*
 *  linux/arch/arm/mach-omap2/powerdomains.c - Contains common powerdomain framework functions
 *
 *  Copyright (C) 2010 Texas Instruments, Inc.
 *  Copyright (C) 2010 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include "pm.h"
#include "cm.h"
#include "cm-regbits-34xx.h"
#include "cm-regbits-44xx.h"
#include "prm-regbits-34xx.h"
#include "prm-regbits-44xx.h"

/* Internal functions */

/* OMAP3 and OMAP4 specific register bit initialisations
 * Notice that the names here are not according to each power
 * domain but the bit mapping used applies to all of them
 */
/* OMAP3 and OMAP4 Memory Onstate Masks (common across all power domains) */
#define OMAP_MEM0_ONSTATE_MASK OMAP3430_SHAREDL1CACHEFLATONSTATE_MASK
#define OMAP_MEM1_ONSTATE_MASK OMAP3430_L1FLATMEMONSTATE_MASK
#define OMAP_MEM2_ONSTATE_MASK OMAP3430_SHAREDL2CACHEFLATONSTATE_MASK
#define OMAP_MEM3_ONSTATE_MASK OMAP3430_L2FLATMEMONSTATE_MASK
#define OMAP_MEM4_ONSTATE_MASK OMAP4430_OCP_NRET_BANK_ONSTATE_MASK

/* OMAP3 and OMAP4 Memory Retstate Masks (common across all power domains) */
#define OMAP_MEM0_RETSTATE_MASK OMAP3430_SHAREDL1CACHEFLATRETSTATE_MASK
#define OMAP_MEM1_RETSTATE_MASK OMAP3430_L1FLATMEMRETSTATE_MASK
#define OMAP_MEM2_RETSTATE_MASK OMAP3430_SHAREDL2CACHEFLATRETSTATE_MASK
#define OMAP_MEM3_RETSTATE_MASK OMAP3430_L2FLATMEMRETSTATE_MASK
#define OMAP_MEM4_RETSTATE_MASK OMAP4430_OCP_NRET_BANK_RETSTATE_MASK

/* OMAP3 and OMAP4 Memory Status bits */
#define OMAP_MEM0_STATEST_MASK OMAP3430_SHAREDL1CACHEFLATSTATEST_MASK
#define OMAP_MEM1_STATEST_MASK OMAP3430_L1FLATMEMSTATEST_MASK
#define OMAP_MEM2_STATEST_MASK OMAP3430_SHAREDL2CACHEFLATSTATEST_MASK
#define OMAP_MEM3_STATEST_MASK OMAP3430_L2FLATMEMSTATEST_MASK
#define OMAP_MEM4_STATEST_MASK OMAP4430_OCP_NRET_BANK_STATEST_MASK

#if !defined(CONFIG_ARCH_OMAP2) && !defined(CONFIG_ARCH_OMAP3)
struct pwrdm_functions omap2_pwrdm_functions;
#elif !defined(CONFIG_ARCH_OMAP4)
struct pwrdm_functions omap4_pwrdm_functions;
#endif

int _get_mem_bank_onstate_mask(u8 bank)
{
	switch (bank) {
	case 0:
		return OMAP_MEM0_ONSTATE_MASK;
	case 1:
		return OMAP_MEM1_ONSTATE_MASK;
	case 2:
		return OMAP_MEM2_ONSTATE_MASK;
	case 3:
		return OMAP_MEM3_ONSTATE_MASK;
	case 4:
		return OMAP_MEM4_ONSTATE_MASK;
	default:
		WARN_ON(1); /* should never happen */
		return -EEXIST;
	}
	return 0;
}

int _get_mem_bank_retst_mask(u8 bank)
{
	switch (bank) {
	case 0:
		return OMAP_MEM0_RETSTATE_MASK;
	case 1:
		return OMAP_MEM1_RETSTATE_MASK;
	case 2:
		return OMAP_MEM2_RETSTATE_MASK;
	case 3:
		return OMAP_MEM3_RETSTATE_MASK;
	case 4:
		return OMAP_MEM4_RETSTATE_MASK;
	default:
		WARN_ON(1); /* should never happen */
		return -EEXIST;
	}
	return 0;
}

int _get_mem_bank_stst_mask(u8 bank)
{
	switch (bank) {
	case 0:
		return OMAP_MEM0_STATEST_MASK;
	case 1:
		return OMAP_MEM1_STATEST_MASK;
	case 2:
		return OMAP_MEM2_STATEST_MASK;
	case 3:
		return OMAP_MEM3_STATEST_MASK;
	case 4:
		return OMAP_MEM4_STATEST_MASK;
	default:
		WARN_ON(1); /* should never happen */
		return -EEXIST;
	}
	return 0;
}

int _get_mem_bank_lastmemst_mask(u8 bank)
{
	switch (bank) {
	case 0:
		return OMAP3430_LASTMEM1STATEENTERED_MASK;
	case 1:
		return OMAP3430_LASTMEM2STATEENTERED_MASK;
	case 2:
		return OMAP3430_LASTSHAREDL2CACHEFLATSTATEENTERED_MASK;
	case 3:
		return OMAP3430_LASTL2FLATMEMSTATEENTERED_MASK;
	case 4:
		return OMAP4430_OCP_NRET_BANK_STATEST_MASK;
	default:
		WARN_ON(1); /* should never happen */
		return -EEXIST;
	}
	return 0;
}
