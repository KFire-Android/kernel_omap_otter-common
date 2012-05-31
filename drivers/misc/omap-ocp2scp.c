/*
 * omap-ocp2scp.c - transform ocp interface protocol to scp protocol
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/omap_ocp2scp.h>

/**
 * _count_resources - count for the number of resources
 * @res: struct resource *
 *
 * Count and return the number of resources populated for the device that is
 * connected to ocp2scp.
 */
static unsigned _count_resources(struct resource *res)
{
	int cnt	= 0;

	while (res->start != res->end) {
		cnt++;
		res++;
	}

	return cnt;
}

static int ocp2scp_remove_devices(struct device *dev, void *c)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int __devinit omap_ocp2scp_probe(struct platform_device *pdev)
{
	int					ret;
	unsigned				res_cnt, i;
	struct platform_device			*pdev_child;
	struct omap_ocp2scp_platform_data	*pdata;
	struct omap_ocp2scp_dev			*dev;

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(&pdev->dev, "OCP2SCP initialized without plat data\n");
		return -EINVAL;
	}

	for (i = 0, dev = *pdata->devices; i < pdata->dev_cnt; i++, dev++) {
		res_cnt = _count_resources(dev->res);

		pdev_child = platform_device_alloc(dev->drv_name, -1);
		if (!pdev_child) {
			dev_err(&pdev->dev, "failed to allocate memory for ocp2scp child\n");
			return -ENOMEM;
		}

		ret = platform_device_add_resources(pdev_child, dev->res,
								res_cnt);
		if (ret) {
			dev_err(&pdev->dev, "failed to add resources for ocp2scp child\n");
			goto err0;
		}

		pdev_child->dev.parent	= &pdev->dev;

		ret = platform_device_add(pdev_child);
		if (ret) {
			dev_err(&pdev->dev, "failed to register ocp2scp child device\n");
			goto err0;
		}
	}

	pm_runtime_enable(&pdev->dev);

	return 0;

err0:
	platform_device_put(pdev_child);
	device_for_each_child(&pdev->dev, NULL, ocp2scp_remove_devices);

	return ret;
}

static int __devexit omap_ocp2scp_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	device_for_each_child(&pdev->dev, NULL, ocp2scp_remove_devices);

	return 0;
}

static struct platform_driver omap_ocp2scp_driver = {
	.probe		= omap_ocp2scp_probe,
	.remove		= __devexit_p(omap_ocp2scp_remove),
	.driver		= {
		.name	= "omap-ocp2scp",
		.owner	= THIS_MODULE,
	},
};

static int __init omap_ocp2scp_init(void)
{
	return platform_driver_register(&omap_ocp2scp_driver);
}
arch_initcall(omap_ocp2scp_init);

static void __exit omap_ocp2scp_exit(void)
{
	platform_driver_unregister(&omap_ocp2scp_driver);
}
module_exit(omap_ocp2scp_exit);

MODULE_ALIAS("platform: omap-ocp2scp");
MODULE_AUTHOR("Kishon Vijay Abraham I <kishon@ti.com>");
MODULE_DESCRIPTION("OMAP OCP2SCP DRIVER");
MODULE_LICENSE("GPL");
