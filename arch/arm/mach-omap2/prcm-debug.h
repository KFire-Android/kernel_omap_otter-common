/*
 * OMAP5 PRCM Debugging
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ASM_MACH_OMAP2_PRCM_DEBUG_H
#define __ARCH_ASM_MACH_OMAP2_PRCM_DEBUG_H

#define PRCMDEBUG_LASTSLEEP	(1 << 0)
#define PRCMDEBUG_ON		(1 << 1)

#ifdef CONFIG_PM_DEBUG
extern void prcmdebug_dump(int flags);
#else
static inline void prcmdebug_dump(int flags) { }
#endif

#endif /* __ARCH_ASM_MACH_OMAP2_PRCM_DEBUG_H */
