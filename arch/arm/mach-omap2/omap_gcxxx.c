/*
 * OMAP gcxxx device initialization
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/gccore.h>
#include <linux/opp.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/omap_gcx.h>
#include <plat/omap-pm.h>
#include "prcm44xx.h"
#include "cminst44xx.h"
#include "cm2_44xx.h"
#include "cm-regbits-44xx.h"
#include "dvfs.h"

static int gcxxx_scale_dev(struct device *dev, unsigned long val);
static int gcxxx_set_l3_bw(struct device *dev, unsigned long val);

static struct omap_gcx_platform_data omap_gcxxx = {
	.was_context_lost = omap_pm_was_context_lost,
	.scale_dev = gcxxx_scale_dev,
	.set_bw = gcxxx_set_l3_bw,
};

struct omap_device_pm_latency omap_gcxxx_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	}
};

static int gcxxx_scale_dev(struct device *dev, unsigned long val)
{
	return omap_device_scale(dev, dev, val);
}

static int gcxxx_set_l3_bw(struct device *dev, unsigned long val)
{
	return omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, val);
}

int __init gcxxx_init(void)
{
	int retval = 0;
	struct omap_hwmod *oh;
	struct omap_device *od;
	const char *oh_name = "bb2d";
	const char *dev_name = "gccore";

	if (!cpu_is_omap447x())
		return retval;

	/*
	 * Hwmod lookup will fail in case our platform doesn't support the
	 * hardware spinlock module, so it is safe to run this initcall
	 * on all omaps
	 */
	oh = omap_hwmod_lookup(oh_name);
	if (oh == NULL)
		return -EINVAL;

	omap_gcxxx.regbase = omap_hwmod_get_mpu_rt_va(oh);
	od = omap_device_build(dev_name, 0, oh, &omap_gcxxx,
				sizeof(omap_gcxxx), omap_gcxxx_latency,
				ARRAY_SIZE(omap_gcxxx_latency), false);
	if (IS_ERR(od)) {
		pr_err("Can't build omap_device for %s:%s\n", dev_name,
								oh_name);
		retval = PTR_ERR(od);
	}

	return retval;
}
arch_initcall(gcxxx_init);

MODULE_AUTHOR("David Sin");
MODULE_DESCRIPTION("omap gcxxx: omap device registration");
MODULE_LICENSE("GPL v2");
