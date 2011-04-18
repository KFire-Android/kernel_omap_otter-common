/*
 * Smartreflex Class 1.5 Routines
 *
 * Copyright (C) 2010-2011 Texas Instruments, Inc.
 * Nishanth Menon <nm@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_SMARTREFLEXCLASS1P5_H
#define __ARCH_ARM_MACH_OMAP2_SMARTREFLEXCLASS1P5_H

#ifdef CONFIG_OMAP_SMARTREFLEX_CLASS1P5
int sr_class1p5_init(void);
#else
static int inline sr_class1p5_init(void) { return 0; }
#endif

#endif
