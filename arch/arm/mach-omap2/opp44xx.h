/*
 * OMAP4 OPP API's.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated.
 *	Thara Gopinath
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * History:
 */

#ifndef __ARCH_ARM_MACH_OMAP2_OPP44XX_H
#define __ARCH_ARM_MACH_OMAP2_OPP44XX_H

#ifdef CONFIG_PM
extern int omap4_pm_init_opp_table(void);
#else
static inline int omap4_pm_init_opp_table(void) { return -EINVAL; }
#endif

#endif
