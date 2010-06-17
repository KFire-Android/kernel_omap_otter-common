/*
 * Runtime PM support code for OMAP1
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
	struct clk *iclk, *fclk;

	dev_dbg(dev, "%s\n", __func__);

	if (dev->driver->pm && dev->driver->pm->runtime_suspend)
		ret = dev->driver->pm->runtime_suspend(dev);

	fclk = clk_get(dev, "fck");
	if (!IS_ERR(fclk)) {
		clk_disable(fclk);
		clk_put(fclk);
	}

	iclk = clk_get(dev, "ick");
	if (!IS_ERR(iclk)) {
		clk_disable(iclk);
		clk_put(iclk);
	}

	return 0;
};

int platform_pm_runtime_resume(struct device *dev)
{
	int r, ret = 0;
	struct clk *iclk, *fclk;

	iclk = clk_get(dev, "ick");
	if (!IS_ERR(iclk)) {
		clk_enable(iclk);
		clk_put(iclk);
	}

	fclk = clk_get(dev, "fck");
	if (!IS_ERR(fclk)) {
		clk_enable(fclk);
		clk_put(fclk);
	}

	if (dev->driver->pm && dev->driver->pm->runtime_resume)
		ret = dev->driver->pm->runtime_resume(dev);

	return ret;
};

int platform_pm_runtime_idle(struct device *dev)
{
	ret = pm_runtime_suspend(dev);
	dev_dbg(dev, "%s [%d]\n", __func__, ret);

	return 0;
};
#endif /* CONFIG_PM_RUNTIME */
