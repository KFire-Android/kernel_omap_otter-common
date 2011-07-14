/*
 * arch/arm/mach-omap2/board-44xx-tablet2.h
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

#ifndef _MACH_OMAP_BOARD_44XX_TABLET2_H
#define _MACH_OMAP_BOARD_44XX_TABLET2_H

int tablet2_touch_init(void);
void omap4_create_board_props(void);
void omap_ion_init(void);
void omap4_register_ion(void);

#endif
