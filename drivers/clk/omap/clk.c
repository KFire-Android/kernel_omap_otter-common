/*
 * Texas Instruments OMAP Clock driver
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon <nm@ti.com>
 *	Tony Lindgren <tony@atomide.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/string.h>

static const struct of_device_id omap_clk_of_match[] = {
	{.compatible = "ti,omap-clock",},
	{},
};

/**
 * omap_clk_src_get() - Get OMAP clock from node name when needed
 * @clkspec:	clkspec argument
 * @data:	unused
 *
 * REVISIT: We assume the following:
 * 1. omap clock names end with _ck
 * 2. omap clock names are under 32 characters in length
 */
static struct clk *omap_clk_src_get(struct of_phandle_args *clkspec, void *data)
{
	struct clk *clk;
	char clk_name[32];
	struct device_node *np = clkspec->np;

	snprintf(clk_name, 32, "%s_ck", np->name);
	clk = clk_get(NULL, clk_name);
	if (IS_ERR(clk))
		pr_err("%s: could not get clock %s(%ld)\n", __func__,
		       clk_name, PTR_ERR(clk));

	return clk;
}

/**
 * omap_clk_probe() - create link from DT definition to clock data
 * @pdev:	device node
 *
 * NOTE: we look up the clock lazily when the consumer driver does
 * of_clk_get() and initialize a NULL clock here.
 */
static int omap_clk_probe(struct platform_device *pdev)
{
	int res;
	struct device_node *np = pdev->dev.of_node;

	/* This allows the driver to of_clk_get() */
	res = of_clk_add_provider(np, omap_clk_src_get, NULL);
	if (res)
		dev_err(&pdev->dev, "could not add provider(%d)\n", res);

	return res;
}

static struct platform_driver omap_clk_driver = {
	.probe = omap_clk_probe,
	.driver = {
		   .name = "omap_clk",
		   .of_match_table = of_match_ptr(omap_clk_of_match),
		   },
};

static int __init omap_clk_init(void)
{
	return platform_driver_register(&omap_clk_driver);
}
arch_initcall(omap_clk_init);

MODULE_DESCRIPTION("OMAP Clock driver");
MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_LICENSE("GPL v2");
