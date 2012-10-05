/*
 * arch/arm/mach-omap2/android_display.h
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Author: Lajos Molnar
 *
 * OMAP2 platform device setup/initialization
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ARCH_ARM_MACH_OMAP_ANDROID_DISPLAY_H
#define __ARCH_ARM_MACH_OMAP_ANDROID_DISPLAY_H

#include <linux/omapfb.h>
#include <video/omapdss.h>
#include <plat/sgx_omaplfb.h>
#include <plat/dsscomp.h>

#define NUM_ANDROID_TILER1D_SLOTS 2

#ifdef CONFIG_ION_OMAP
int omap_android_display_setup(struct omap_dss_board_info *dss,
				struct dsscomp_platform_data *dsscomp,
				struct sgx_omaplfb_platform_data *sgx,
				struct omapfb_platform_data *fb);

bool omap_android_display_is_default(struct omap_dss_device *device);
#else
static inline void omap_android_display_setup(struct omap_dss_board_info *dss,
				struct dsscomp_platform_data *dsscomp,
				struct sgx_omaplfb_platform_data *sgx,
				struct omapfb_platform_data *fb)
{
}

static inline bool omap_android_display_is_default(struct omap_dss_device *dev)
{
	return false;
}
#endif

#endif
