/*
 * OMAP ram console support
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Huzefa Kankroliwala <huzefank@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __OMAP_RAM_CONSOLE_H
#define __OMAP_RAM_CONSOLE_H

/* Board files use the following if they are ok with the defaults */
#define OMAP_RAM_CONSOLE_START_DEFAULT	(PLAT_PHYS_OFFSET + SZ_256M)
#define OMAP_RAM_CONSOLE_SIZE_DEFAULT	SZ_2M

#ifdef CONFIG_OMAP_RAM_CONSOLE
extern int omap_ram_console_init(phys_addr_t phy_addr, size_t size);
#else
static inline int omap_ram_console_init(phys_addr_t phy_addr, size_t size)
{
	return 0;
}
#endif /* CONFIG_OMAP_RAM_CONSOLE */

#endif
