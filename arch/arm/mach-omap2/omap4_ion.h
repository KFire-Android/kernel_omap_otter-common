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

#ifdef CONFIG_ION_OMAP
void omap_ion_init(void);
void omap4_register_ion(void);
#else
inline void omap_ion_init(void) { return; }
inline void omap4_register_ion(void) { return; }
#endif

#endif
