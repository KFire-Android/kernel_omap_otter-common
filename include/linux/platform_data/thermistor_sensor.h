/*
 * Thermistor temperature sensor header file
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_THERMISTOR_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_THERMISTOR_H

/*
 * thermistor sensor platform data
 * @name - name
 * @slope - slope for hotspot temperature calculation
 * @offset - offset for hotspot temperature calculation
 * @domain - domain where sensor should be registered
 */
struct thermistor_pdata {
	const char *name;
	int slope;
	int offset;
	const char *domain;
};

#endif
