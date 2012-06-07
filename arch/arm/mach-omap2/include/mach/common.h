/*
 * Common header to contain shared information with core drivers
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_ARM_MACH_OMAP2_MACH_COMMON_H_
#define __ARCH_ARM_MACH_OMAP2_MACH_COMMON_H_

#if defined(CONFIG_ARCH_OMAP4) || defined(CONFIG_ARCH_OMAP5)
int omap4_prcm_freq_update(void);
#else
static inline int omap4_prcm_freq_update(void)
{
	return -EINVAL;
}
#endif

#endif			/* __ARCH_ARM_MACH_OMAP2_MACH_COMMON_H_ */
