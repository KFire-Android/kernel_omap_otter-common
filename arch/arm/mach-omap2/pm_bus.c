/*
 * Runtime PM support code for OMAP
 *
 * Author: Kevin Hilman, Deep Root Systems, LLC
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

#include <plat/omap_device.h>
#include <plat/omap-pm.h>

#ifdef CONFIG_PM_RUNTIME
int platform_pm_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_device *odev = to_omap_device(pdev);
	int r, ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	if (dev->driver->pm && dev->driver->pm->runtime_suspend)
		ret = dev->driver->pm->runtime_suspend(dev);
	if (!ret && omap_device_is_valid(odev)) {
		r = omap_device_idle(pdev);
		WARN_ON(r);
	}

	return ret;
};

int platform_pm_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_device *odev = to_omap_device(pdev);
	int r, ret = 0;

	dev_dbg(dev, "%s\n", __func__);

	if (omap_device_is_valid(odev)) {
		r = omap_device_enable(pdev);
		WARN_ON(r);
	}

	if (dev->driver->pm && dev->driver->pm->runtime_resume)
		ret = dev->driver->pm->runtime_resume(dev);

	return ret;
};

int platform_pm_runtime_idle(struct device *dev)
{
	int ret;

	ret = pm_runtime_suspend(dev);
	dev_dbg(dev, "%s [%d]\n", __func__, ret);

	return 0;
};
#endif /* CONFIG_PM_RUNTIME */

