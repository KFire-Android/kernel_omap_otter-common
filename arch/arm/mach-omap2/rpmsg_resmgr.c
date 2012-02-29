/*
 * Remote processor Resource Manager  machine-specific module for OMAP4
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <plat/rpmsg_resmgr.h>
#include <plat/omap-pm.h>

static int omap2_rprm_set_min_bus_tput(struct device *rdev,
		struct device *tdev, unsigned long val)
{
	return omap_pm_set_min_bus_tput(rdev, OCP_INITIATOR_AGENT, val);
}

static int omap2_rprm_set_max_dev_wakeup_lat(struct device *rdev,
		struct device *tdev, unsigned long val)
{
	return omap_pm_set_max_dev_wakeup_lat(rdev, tdev, val);
}

#ifdef CONFIG_OMAP_DVFS
static int omap2_rprm_device_scale(struct device *rdev, struct device *tdev,
		unsigned long val);
{
	return omap_device_scale(dev, &pdev->dev, val);
}
#else
#define omap2_rprm_device_scale NULL
#endif

static struct omap_rprm_ops omap2_rprm_ops = {
	.set_min_bus_tput	= omap2_rprm_set_min_bus_tput,
	.set_max_dev_wakeup_lat	= omap2_rprm_set_max_dev_wakeup_lat,
	.device_scale		= omap2_rprm_device_scale,
};

static struct omap_rprm_pdata omap2_rprm_data = {
	.ops		= &omap2_rprm_ops,
};

static int __init omap2_rprm_init(void)
{
	struct platform_device *pdev;
	int ret;

	pdev = platform_device_alloc("omap-rprm", -1);
	if (!pdev)
		return -ENOMEM;

	ret = platform_device_add_data(pdev, &omap2_rprm_data,
			sizeof omap2_rprm_data);
	if (ret)
		goto err;

	ret =  platform_device_register(pdev);
	if (ret)
		goto err;

	return 0;

err:
	platform_device_del(pdev);
	return ret;
}
device_initcall(omap2_rprm_init);
