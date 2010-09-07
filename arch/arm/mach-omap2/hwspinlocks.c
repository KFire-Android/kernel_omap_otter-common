/*
 * OMAP hardware spinlock device initialization
 *
 * Copyright (C) 2010 Texas Instruments. All rights reserved.
 *
 * Contact: Simon Que <sque@ti.com>
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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <plat/hwspinlock.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>

/* Spinlock register offsets */
#define REVISION_OFFSET			0x0000
#define SYSCONFIG_OFFSET		0x0010
#define SYSSTATUS_OFFSET		0x0014
#define LOCK_BASE_OFFSET		0x0800
#define LOCK_OFFSET(i)			(LOCK_BASE_OFFSET + 0x4 * (i))

struct omap_device_pm_latency omap_spinlock_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	}
};

/* Initialization function */
int __init hwspinlocks_init(void)
{
	int retval = 0;

	struct hwspinlock_plat_info *pdata;
	struct omap_hwmod *oh;
	char *oh_name, *pdev_name;

	if (!cpu_is_omap44xx())
		return -EINVAL;

	oh_name = "spinlock";
	oh = omap_hwmod_lookup(oh_name);
	if (WARN_ON(oh == NULL))
		return -EINVAL;

	pdev_name = "hwspinlock";

	/* Pass data to device initialization */
	pdata = kzalloc(sizeof(struct hwspinlock_plat_info), GFP_KERNEL);
	if (WARN_ON(pdata == NULL))
		return -ENOMEM;
	pdata->sysstatus_offset = SYSSTATUS_OFFSET;
	pdata->lock_base_offset = LOCK_BASE_OFFSET;

	omap_device_build(pdev_name, 0, oh, pdata,
		sizeof(struct hwspinlock_plat_info), omap_spinlock_latency,
		ARRAY_SIZE(omap_spinlock_latency), false);

	return retval;
}
postcore_initcall(hwspinlocks_init);
