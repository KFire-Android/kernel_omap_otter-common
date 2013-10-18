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

#ifndef _MACH_OMAP_BOARD_BOWSER_H
#define _MACH_OMAP_BOARD_BOWSER_H

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_TTSP
#define GPIO_TOUCH_RESET 23
#define GPIO_TOUCH_IRQ   24
#endif

//int bowser_sensor_init(void);
int bowser_panel_init(void);
//int bowser_keypad_init(void);
void omap4_create_board_props(void);

extern struct mmc_platform_data bowser_wifi_data;
extern int bowser_wifi_init(void);

struct omap_ion_platform_data;
void bowser_android_display_setup(struct omap_ion_platform_data *ion);

#endif
