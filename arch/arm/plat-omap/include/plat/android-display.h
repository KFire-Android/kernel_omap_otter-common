/*
 * arch/arm/mach-omap2/android_display.h
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
#include <mach/omap4_ion.h>
#include <plat/sgx_omaplfb.h>
#include <plat/dsscomp.h>

#define NUM_ANDROID_TILER1D_SLOTS 2

void omap_android_display_setup(struct omap_dss_board_info *dss,
				struct dsscomp_platform_data *dsscomp,
				struct sgx_omaplfb_platform_data *sgx,
				struct omapfb_platform_data *fb,
				struct omap_ion_platform_data *ion);

bool omap_android_display_is_default(struct omap_dss_device *device);

#endif
