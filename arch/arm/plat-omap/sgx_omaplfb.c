/*
 * SGX display class driver platform resources
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
#include <linux/slab.h>
#include <plat/board.h>

#include <plat/sgx_omaplfb.h>

#if defined(CONFIG_FB_OMAP2_NUM_FBS)
#define OMAPLFB_NUM_DEV CONFIG_FB_OMAP2_NUM_FBS
#else
#define OMAPLFB_NUM_DEV 1
#endif

static struct sgx_omaplfb_config omaplfb_config[OMAPLFB_NUM_DEV] = {
	{
	.tiler2d_buffers = 2,
	.swap_chain_length = 2,
	}
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data = {
	.num_configs = OMAPLFB_NUM_DEV,
	.configs = omaplfb_config,
};

static struct platform_device omaplfb_plat_device = {
	.name		= "omaplfb",
	.id		= -1,
	.dev = {
		.platform_data		= &omaplfb_plat_data,
	},
	.num_resources = 0,
};

int sgx_omaplfb_set(unsigned int fbix, struct sgx_omaplfb_config *data)
{
	if (fbix >= OMAPLFB_NUM_DEV) {
		WARN(1, "Invalid FB device index");
		return -ENOENT;
	}
	omaplfb_config[fbix] = *data;
	return 0;
}

struct sgx_omaplfb_config *sgx_omaplfb_get(unsigned int fbix)
{
	if (fbix >= OMAPLFB_NUM_DEV) {
		WARN(1, "Invalid FB device index");
		return NULL;
	}
	return &omaplfb_config[fbix];
}

static int __init omap_init_omaplfb(void)
{
	return platform_device_register(&omaplfb_plat_device);
}

arch_initcall(omap_init_omaplfb);
