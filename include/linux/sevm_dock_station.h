/*
 * sEVM Dock Station function prototypes and macros
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Author: Leed Aguilar <leed.aguilar@ti.com>
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

#ifndef _MACH_OMAP_SEVM_DOCK_STATION_H
#define _MACH_OMAP_SEVM_DOCK_STATION_H

#define DOCK_STATION_NAME	"dock-station"

/**
 * struct sevm_dockstation_platform_data - sEVM dock station platform data
 * @def_poll_rate: poll rate value for work schedule routine
 */
struct sevm_dockstation_platform_data {
	int gpio;
	int irqflags;
};

#endif
