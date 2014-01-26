/*
 * TI DRA7xx Radio Helper Sub-driver
 *
 * Copyright (C) 2013-2014 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Ravikumar Kattekola <rk@ti.com>
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
#include <linux/of.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>

static int dra7xx_radio_subdev_probe(struct platform_device *pdev)
{
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
	pr_info("%s: Probe done\n", pdev->name);
	return 0;
}

static int dra7xx_radio_subdev_remove(struct platform_device *pdev)
{
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id dra7xx_radio_subdev_of_match[] = {
	{ .compatible = "ti,dra7xx_radio_subdev", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, dra7xx_radio_subdev_of_match);

static struct platform_driver dra7xx_radio_subdev_driver = {
	.driver	= {
		.of_match_table = of_match_ptr(dra7xx_radio_subdev_of_match),
		.name	= "dra7xx_radio_subdev",
		.owner	= THIS_MODULE,
	},
	.probe = dra7xx_radio_subdev_probe,
	.remove = dra7xx_radio_subdev_remove,
};

module_platform_driver(dra7xx_radio_subdev_driver);

MODULE_DESCRIPTION("DRA7xx Radio Helper Sub-driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dra7xx_radio_helper");
MODULE_AUTHOR("Texas Instrument Inc.");
