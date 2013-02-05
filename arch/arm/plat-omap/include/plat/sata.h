/**
 * sata.h - The ahci sata device init function prototype
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 * Author: Keshava Munegowda <keshava_mgowda@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PLAT_ARCH_OMAP_SATA_H
#define __PLAT_ARCH_OMAP_SATA_H

#if IS_ENABLED(CONFIG_SATA_AHCI_PLATFORM)
extern void omap_sata_init(void);
#else
static inline void omap_sata_init(void)
{  }
#endif

#endif
