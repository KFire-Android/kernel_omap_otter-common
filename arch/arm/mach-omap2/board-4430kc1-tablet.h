/*
 * arch/arm/mach-omap2/board-4430kc1-tablet.h
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

#ifndef _MACH_OMAP_BOARD_4430KC1_TABLET_H
#define _MACH_OMAP_BOARD_4430KC1_TABLET_H

#define OMAP4_CHARGER_IRQ		7

void omap4_create_board_props(void);

void omap4_power_init(void);

void board_serial_init(void);

void omap4_kc1_display_init(void);

struct omap_ion_platform_data;
void omap4_kc1_android_display_setup(struct omap_ion_platform_data *ion);

#endif
