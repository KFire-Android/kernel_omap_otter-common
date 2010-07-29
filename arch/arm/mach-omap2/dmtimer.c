/**
 * linux/arch/arm/mach-omap2/dmtimers.c
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Thara Gopinath <thara@ti.com>
 * Tarun Kanti DebBarma <tarun.kanti@ti.com>
 * 	- Highlander ip support on omap4
 *	- hwmod support
 *
 * OMAP2 Dual-Mode Timers
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <mach/irqs.h>
#include <plat/dmtimer.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <linux/pm_runtime.h>

static int timer_id __initdata;

static char *omap2_dm_source_names[] __initdata = {
	"sys_ck",
	"func_32k_ck",
	"alt_ck",
	NULL
};
static struct clk *omap2_dm_source_clocks[3];

static char *omap3_dm_source_names[] __initdata = {
	"sys_ck",
	"omap_32k_fck",
	NULL
};
static struct clk *omap3_dm_source_clocks[2];

static char *omap4_dm_source_names[] __initdata = {
	"sys_clkin_ck",
	"sys_32k_ck",
	"syc_clk_div_ck",
	NULL
};
static struct clk *omap4_dm_source_clocks[3];

static struct clk **omap_dm_source_clocks;

static void omap2_dm_timer_enable(struct platform_device *pdev)
{
#ifdef CONFIG_PM_RUNTIME
	if (pm_runtime_get_sync(&pdev->dev))
		dev_warn(&pdev->dev, "%s: Unable to enable the timer%d\n",
			__func__, pdev->id);
#endif
}

static void omap2_dm_timer_disable(struct platform_device *pdev)
{
#ifdef CONFIG_PM_RUNTIME
	if (pm_runtime_put_sync(&pdev->dev))
		dev_warn(&pdev->dev, "%s: Unable to disable the timer%d\n",
			__func__, pdev->id);
#endif
}

static void omap2_dm_ms_timer_enable(struct platform_device *pdev)
{
#ifdef CONFIG_PM_RUNTIME
	/* when pm_runtime is enabled, it is still inactive at this point
	 * that is why this call is needed as it is not enabled by default
	 */
	if (omap_device_enable(pdev))
		dev_warn(&pdev->dev, "%s: Unable to enable the timer%d\n",
			__func__, pdev->id);
#endif
}

static void omap2_dm_ms_timer_disable(struct platform_device *pdev)
{
#ifdef CONFIG_PM_RUNTIME
	/* when pm_runtime is enabled, it is still inactive at this point
	 * that is why this call is needed as it is not enabled by default
	 */
	if (omap_device_idle(pdev))
		dev_warn(&pdev->dev, "%s: Unable to disable the timer%d\n",
			__func__, pdev->id);
#endif
}

static int omap2_dm_timer_set_src(struct platform_device *pdev,
			struct clk *timer_clk, int source)
{
	int ret;

	if (IS_ERR(timer_clk)) {
		dev_warn(&pdev->dev, "%s: Not able get the clock pointer\n",
			__func__);
		return -EINVAL;
	}
	clk_disable(timer_clk);
	ret = clk_set_parent(timer_clk, omap_dm_source_clocks[source]);
	if (ret)
		dev_warn(&pdev->dev, "%s: Not able to change "
			"fclk source\n", __func__);
	clk_enable(timer_clk);

	/*
	 * When the functional clock disappears, too quick writes seem
	 * to cause an abort. XXX Is this still necessary?
	 */
	__delay(150000);
	return ret;
}

static int omap2_dm_timer_set_clk(struct platform_device *pdev, int source)
{
	struct clk *timer_clk = clk_get(&pdev->dev, "fck");

	return omap2_dm_timer_set_src(pdev, timer_clk, source);
}

struct clk *omap2_dm_timer_get_fclk(struct platform_device *pdev)
{
	return clk_get(&pdev->dev, "fck");
}

/* One time initializations */
static void __init omap2_dm_timer_setup(void)
{
	int i;
	char **clock_source_names;

	if (cpu_is_omap24xx()) {
		clock_source_names = omap2_dm_source_names;
		omap_dm_source_clocks = omap2_dm_source_clocks;
	} else if (cpu_is_omap34xx()) {
		clock_source_names = omap3_dm_source_names;
		omap_dm_source_clocks = omap3_dm_source_clocks;
	} else if (cpu_is_omap44xx()) {
		clock_source_names = omap4_dm_source_names;
		omap_dm_source_clocks = omap4_dm_source_clocks;
	} else {
		pr_err("%s: Chip support not yet added\n", __func__);
		return;
	}

	/* Initialize the dmtimer src clocks */
	for (i = 0; clock_source_names[i] != NULL; i++)
			omap_dm_source_clocks[i] =
				clk_get(NULL, clock_source_names[i]);
}

struct omap_device_pm_latency omap2_dmtimer_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int __init omap_dm_timer_early_init(struct omap_hwmod *oh, void *user)
{
	int id;
	char *name = "dmtimer";
	struct omap_dmtimer_platform_data *pdata;
	struct omap_device *od;

	if (!oh) {
		pr_err("%s:Could not find [%s]\n", __func__, oh->name);
	return -EINVAL;
	}

	id = timer_id;
	pr_debug("%s:%s\n", __func__, oh->name);

	pdata = kzalloc(sizeof(struct omap_dmtimer_platform_data),
			GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: No memory for [%s]\n", __func__, oh->name);
		return -ENOMEM;
	}
	pdata->omap_dm_clk_enable = omap2_dm_ms_timer_enable;
	pdata->omap_dm_clk_disable = omap2_dm_ms_timer_disable;
	pdata->omap_dm_set_source_clk = omap2_dm_timer_set_clk;
	pdata->omap_dm_get_timer_clk = omap2_dm_timer_get_fclk;

	pdata->timer_ip_type = oh->class->rev;

	if (unlikely(!strcmp(oh->name, "timer10")))
		id = 9;

	od = omap_device_build(name, id, oh, pdata, sizeof(*pdata),
			omap2_dmtimer_latency,
			ARRAY_SIZE(omap2_dmtimer_latency), 1);
	if (IS_ERR(od)) {
		pr_err("%s: Cant build omap_device for %s:%s.\n",
			__func__, name, oh->name);
		kfree(pdata);
	}
	timer_id++;
	return 0;
}

static int __init omap2_dm_timer_init(struct omap_hwmod *oh, void *user)
{
	int id;
	char *name = "dmtimer";
	struct omap_device *od;
	struct omap_dmtimer_platform_data *pdata;

	if (!oh) {
		pr_err("%s:NULL hwmod pointer (oh)\n", __func__);
		return -EINVAL;
	}

	id = timer_id;
	pr_debug("%s:%s\n", __func__, oh->name);

	pdata = kzalloc(sizeof(struct omap_dmtimer_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s:No memory for [%s]\n",  __func__, oh->name);
		return -ENOMEM;
	}
	/* hook clock set/get functions */
	pdata->omap_dm_set_source_clk = omap2_dm_timer_set_clk;
	pdata->omap_dm_get_timer_clk = omap2_dm_timer_get_fclk;

	/* hook timer enable/disable functions */
	pdata->omap_dm_clk_enable = omap2_dm_timer_enable;
	pdata->omap_dm_clk_disable = omap2_dm_timer_disable;

	/* Update Highlander offsets for GPTimer [3-9, 11-12].
	 * The offset1 & offset2 will be zero on OMAP3 and
	 * OMAP4 millisecond timers (GPT1, GPT2, GPT10).
	 */
	pdata->timer_ip_type = oh->class->rev;

	if (unlikely(!strcmp(oh->name, "timer10")))
		id = 9;
	else
		timer_id++;

	if (unlikely(!strcmp(oh->name, "timer11"))) {
		id = 10;
		timer_id++;
	}

	od = omap_device_build(name, id, oh,
			pdata, sizeof(*pdata),
			omap2_dmtimer_latency,
			ARRAY_SIZE(omap2_dmtimer_latency), 0);
	WARN(IS_ERR(od), "Cant build omap_device for %s:%s.\n",
			name, oh->name);

	return 0;
}

void __init omap2_dm_timer_early_init(void)
{
	timer_id = 0;
	omap_hwmod_for_each_by_class("timer_1ms",
				omap_dm_timer_early_init, NULL);
	omap2_dm_timer_setup();
	early_platform_driver_register_all("earlytimer");
	early_platform_driver_probe("earlytimer", timer_id+1, 0);
}

static int __init omap_timer_init(void)
{
	timer_id = 0;
	/* register all timers again */
	omap_hwmod_for_each_by_class("timer_1ms", omap2_dm_timer_init, NULL);
	omap_hwmod_for_each_by_class("timer", omap2_dm_timer_init, NULL);
	return 0;
}
arch_initcall(omap_timer_init);
