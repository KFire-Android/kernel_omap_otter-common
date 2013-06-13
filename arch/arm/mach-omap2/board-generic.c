/*
 * Copyright (C) 2005 Nokia Corporation
 * Author: Paul Mundt <paul.mundt@nokia.com>
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Modified from the original mach-omap/omap2/board-generic.c did by Paul
 * to support the OMAP2+ device tree boards with an unique board file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/irqdomain.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <linux/slab.h>

#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>

#include <plat/sata.h>

#include "common.h"
#include "common-board-devices.h"
#include "dss-common.h"

#if !(defined(CONFIG_ARCH_OMAP2) || defined(CONFIG_ARCH_OMAP3))
#define intc_of_init	NULL
#endif
#ifndef CONFIG_ARCH_OMAP4
#define gic_of_init		NULL
#endif

static struct of_device_id omap_dt_match_table[] __initdata = {
	{ .compatible = "simple-bus", },
	{ .compatible = "ti,omap-infra", },
	{ }
};

static int __init omap_create_clk_alias(struct device_node *np)
{
	int ret = 0;
	const char *s, *alias;
	char *clk_id;
	struct device_node *dev_np;
	struct platform_device *pdev;

	of_property_read_string(np, "clock-name", &s);
	if (!s) {
		pr_err("%s: couldn't find clock-name property in node %s\n",
				__func__, np->name);
		return -ENODEV;
	}

	clk_id = kstrdup(s, GFP_KERNEL);
	if (!clk_id)
		return -ENOMEM;

	dev_np = of_parse_phandle(np, "device", 0);
	if (!dev_np) {
		pr_err("%s: couldn't find device phandle for \'%s\'\n",
				__func__, clk_id);
		ret = -ENODEV;
		goto exit;
	}

	pdev = of_find_device_by_node(dev_np);
	if (!pdev) {
		pr_err("%s: couldn't find device for clock \'%s\'\n",
				__func__, clk_id);
		ret = -ENODEV;
		goto exit;
	}

	ret = of_property_read_string(np, "clock-alias", &alias);
	if (ret) {
		pr_err("%s: couldn't find alias for clock \'%s\'\n",
				__func__, clk_id);
		ret = -ENODEV;
		goto exit;
	}

	ret = clk_add_alias(alias, dev_name(&pdev->dev), clk_id, NULL);
	if (ret) {
		pr_err("%s: couldn't add alias \'%s\' to clock \'%s\'\n",
				__func__, alias, clk_id);
		ret = -ENODEV;
		goto exit;
	}

exit:
	kfree(clk_id);
	return ret;
}

static void __init omap_generic_init(void)
{
	struct device_node *np;

	omap_sdrc_init(NULL, NULL);

	of_platform_populate(NULL, omap_dt_match_table, NULL, NULL);

	omapdss_init_of();

	/* create clock aliases based on 'clock_alias' nodes */
	for_each_node_by_name(np, "clock_alias") {
		omap_create_clk_alias(np);
		of_node_put(np);
	}

	if (of_machine_is_compatible("ti,omap5"))
		omap_sata_init();
}

#ifdef CONFIG_SOC_OMAP2420
static const char *omap242x_boards_compat[] __initdata = {
	"ti,omap2420",
	NULL,
};

DT_MACHINE_START(OMAP242X_DT, "Generic OMAP2420 (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.map_io		= omap242x_map_io,
	.init_early	= omap2420_init_early,
	.init_irq	= omap_intc_of_init,
	.handle_irq	= omap2_intc_handle_irq,
	.init_machine	= omap_generic_init,
	.timer		= &omap2_timer,
	.dt_compat	= omap242x_boards_compat,
	.restart	= omap2xxx_restart,
MACHINE_END
#endif

#ifdef CONFIG_SOC_OMAP2430
static const char *omap243x_boards_compat[] __initdata = {
	"ti,omap2430",
	NULL,
};

DT_MACHINE_START(OMAP243X_DT, "Generic OMAP2430 (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.map_io		= omap243x_map_io,
	.init_early	= omap2430_init_early,
	.init_irq	= omap_intc_of_init,
	.handle_irq	= omap2_intc_handle_irq,
	.init_machine	= omap_generic_init,
	.timer		= &omap2_timer,
	.dt_compat	= omap243x_boards_compat,
	.restart	= omap2xxx_restart,
MACHINE_END
#endif

#ifdef CONFIG_ARCH_OMAP3
static const char *omap3_boards_compat[] __initdata = {
	"ti,omap3",
	NULL,
};

DT_MACHINE_START(OMAP3_DT, "Generic OMAP3 (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= omap3430_init_early,
	.init_irq	= omap_intc_of_init,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_generic_init,
	.init_late	= omap3_init_late,
	.timer		= &omap3_timer,
	.dt_compat	= omap3_boards_compat,
	.restart	= omap3xxx_restart,
MACHINE_END

static const char *omap3_gp_boards_compat[] __initdata = {
	"ti,omap3-beagle",
	NULL,
};

DT_MACHINE_START(OMAP3_GP_DT, "Generic OMAP3-GP (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.map_io		= omap3_map_io,
	.init_early	= omap3430_init_early,
	.init_irq	= omap_intc_of_init,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_generic_init,
	.init_late	= omap3_init_late,
	.timer		= &omap3_secure_timer,
	.dt_compat	= omap3_gp_boards_compat,
	.restart	= omap3xxx_restart,
MACHINE_END
#endif

#ifdef CONFIG_SOC_AM33XX
static const char *am33xx_boards_compat[] __initdata = {
	"ti,am33xx",
	NULL,
};

DT_MACHINE_START(AM33XX_DT, "Generic AM33XX (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.map_io		= am33xx_map_io,
	.init_early	= am33xx_init_early,
	.init_late	= am33xx_init_late,
	.init_irq	= omap_intc_of_init,
	.handle_irq	= omap3_intc_handle_irq,
	.init_machine	= omap_generic_init,
	.timer		= &omap3_am33xx_timer,
	.dt_compat	= am33xx_boards_compat,
	.restart	= am33xx_restart,
MACHINE_END
#endif

#ifdef CONFIG_ARCH_OMAP4
static const char *omap4_boards_compat[] __initdata = {
	"ti,omap4",
	NULL,
};

DT_MACHINE_START(OMAP4_DT, "Generic OMAP4 (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.smp		= smp_ops(omap4_smp_ops),
	.map_io		= omap4_map_io,
	.init_early	= omap4430_init_early,
	.init_irq	= omap_gic_of_init,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_generic_init,
	.init_late	= omap4430_init_late,
	.timer		= &omap4_timer,
	.dt_compat	= omap4_boards_compat,
	.restart	= omap44xx_restart,
MACHINE_END
#endif

#ifdef CONFIG_SOC_OMAP5
static const char *omap5_boards_compat[] __initdata = {
	"ti,omap5",
	NULL,
};

DT_MACHINE_START(OMAP5_DT, "Generic OMAP5 (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.smp		= smp_ops(omap4_smp_ops),
	.map_io		= omap5_map_io,
	.init_early	= omap5_init_early,
	.init_irq	= omap_gic_of_init,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_generic_init,
	.init_late	= omap5_init_late,
	.timer		= &omap5_timer,
	.dt_compat	= omap5_boards_compat,
	.restart	= omap44xx_restart,
MACHINE_END
#endif

#ifdef CONFIG_SOC_DRA7XX
static const char *dra7xx_boards_compat[] __initdata = {
	"ti,dra7",
	NULL,
};

DT_MACHINE_START(DRA7XX_DT, "Generic DRA7XX (Flattened Device Tree)")
	.reserve	= omap_reserve,
	.smp		= smp_ops(omap4_smp_ops),
	.map_io		= omap5_map_io,
	.init_early	= dra7xx_init_early,
	.init_irq	= omap_gic_of_init,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_generic_init,
	.init_late	= dra7xx_init_late,
	.timer		= &omap5_timer,
	.dt_compat	= dra7xx_boards_compat,
	.restart	= omap44xx_restart,
MACHINE_END
#endif
