/*
 * OMAP3/OMAP4 DVFS Management Routines
 *
 * Author: Vishwanath BS        <vishwanath.bs@ti.com>
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Vishwanath BS <vishwanath.bs@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_DVFS_H
#define __ARCH_ARM_MACH_OMAP2_DVFS_H

#ifdef CONFIG_PM
#include <linux/mutex.h>
extern struct mutex omap_dvfs_lock;

int omap_dvfs_register_device(struct device *dev, char *voltdm_name,
				char *clk_name);
int omap_device_scale(struct device *target_dev, unsigned long rate);
static inline bool omap_dvfs_is_any_dev_scaling(void)
{
	return mutex_is_locked(&omap_dvfs_lock);
}
int omap_opp_register(struct device *dev, const char *hwmod_name);
void omap_dvfs_suspend(bool suspend);

#else
static inline int omap_dvfs_register_device(struct device *dev,
				char *voltdm_name, char *clk_name)
{
	return 0;
}
static inline int omap_device_scale(struct device *target_dev,
				    unsigned long rate)
{
	return 0;
}
static inline bool omap_dvfs_is_any_dev_scaling(void)
{
	return false;
}
void omap_dvfs_suspend(bool suspend) {}
#endif
#endif
