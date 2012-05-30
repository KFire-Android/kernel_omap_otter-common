/*
 * File: arch/arm/plat-omap/omap_dsscomp.c
 *
 * dsscomp resources registration for TI OMAP platforms
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <plat/board.h>

#include <video/dsscomp.h>
#include <plat/dsscomp.h>

static struct dsscomp_platform_data dsscomp_config = {
		.tiler1d_slotsz = SZ_16M,
};

static struct platform_device omap_dsscomp_device = {
	.name		= "dsscomp",
	.id		= -1,
	.dev = {
		.platform_data		= &dsscomp_config,
	},
	.num_resources = 0,
};

void dsscomp_set_platform_data(struct dsscomp_platform_data *data)
{
	dsscomp_config = *data;
}

static int __init omap_init_dsscomp(void)
{
	return platform_device_register(&omap_dsscomp_device);
}

arch_initcall(omap_init_dsscomp);

