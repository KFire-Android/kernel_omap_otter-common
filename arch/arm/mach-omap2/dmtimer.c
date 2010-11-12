/**
 * OMAP2PLUS Dual-Mode Timers - platform device registration
 *
 * Contains first level initialization routines which extracts timers
 * information from hwmod database and registers with linux device model.
 * It also has low level function to chnage the timer input clock source.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 * Tarun Kanti DebBarma <tarun.kanti@ti.com>
 *
 * Copyright (C) 2010 Texas Instruments Incorporated
 * Thara Gopinath <thara@ti.com>
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
#define DEBUG

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

static int early_timer_count __initdata;
static int is_early_init __initdata = 1;

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

static int omap2_dm_timer_set_src(struct platform_device *pdev,
			struct clk *timer_clk, int source)
{
	int ret;

	if (IS_ERR(timer_clk)) {
		dev_warn(&pdev->dev, "%s: Not able get the clock pointer\n",
			__func__);
		return -EINVAL;
	}

#ifndef CONFIG_PM_RUNTIME
	clk_disable(timer_clk); /* enabled in hwmod */
#else
	if (unlikely(is_early_init))
		omap_device_idle(pdev);
	else {
		ret = pm_runtime_put_sync(&pdev->dev);
		if (unlikely(ret))
			dev_warn(&pdev->dev,
			"%s: pm_runtime_put_sync FAILED(%d)\n",
			__func__, ret);
	}
#endif

	ret = clk_set_parent(timer_clk, omap_dm_source_clocks[source]);
	if (ret)
		dev_warn(&pdev->dev, "%s: Not able to change "
			"fclk source\n", __func__);

#ifndef CONFIG_PM_RUNTIME
	clk_enable(timer_clk);
#else
	if (unlikely(is_early_init))
		omap_device_enable(pdev);
	else {
		ret = pm_runtime_get_sync(&pdev->dev);
		if (unlikely(ret))
			dev_warn(&pdev->dev,
			"%s: pm_runtime_get_sync FAILED(%d)\n",
			__func__, ret);
	}
#endif

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

static int omap2_dm_early_timer_set_clk
			(struct platform_device *pdev, int source)
{
	struct omap_device *odev = to_omap_device(pdev);

	return omap2_dm_timer_set_src(pdev,
			omap_hwmod_get_clk(odev->hwmods[0]),
			source);
}

static struct clk *omap2_dm_early_timer_get_fclk
				(struct platform_device *pdev)
{
	struct omap_device *odev = to_omap_device(pdev);

	return omap_hwmod_get_clk(odev->hwmods[0]);
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

static int __init omap2_timer_early_init(struct omap_hwmod *oh, void *user)
{
	int id;
	char *name = "dmtimer";
	struct omap_dmtimer_platform_data *pdata;
	struct omap_device *od;

	if (!oh) {
		pr_err("%s: Could not find [%s]\n", __func__, oh->name);
		return -EINVAL;
	}

	pr_debug("%s:%s\n", __func__, oh->name);

	pdata = kzalloc(sizeof(struct omap_dmtimer_platform_data),
			GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: No memory for [%s]\n", __func__, oh->name);
		return -ENOMEM;
	}
	pdata->omap_dm_set_source_clk = omap2_dm_early_timer_set_clk;
	pdata->omap_dm_get_timer_clk = omap2_dm_early_timer_get_fclk;

	pdata->timer_ip_type = oh->class->rev;

	if (pdata->timer_ip_type == OMAP_TIMER_IP_VERSION_1) {
		pdata->offset1 = 0;
		pdata->offset2 = 0;
	} else {
		pdata->offset1 = 0x10;
		pdata->offset2 = 0x14;
	}

	pdata->is_early_init = 1;

	/*
	 * extract the id from name
	 * this is not the best implementation, but the cleanest with
	 * the existing constraints: (1) early timers are not sequential,
	 * 1/2/10 (2) export APIs use id's to identify corresponding
	 * timers (3) non-early timers initialization have to track the
	 * id's already used by early timers.
	 *
	 * well, the ideal solution would have been to have an 'id' field
	 * in struct omap_hwmod {}. otherwise, we have to have devattr
	 * for all timers.
	 */
	sscanf(oh->name, "timer%2d", &id);

	od = omap_device_build(name, id - 1, oh, pdata, sizeof(*pdata),
			omap2_dmtimer_latency,
			ARRAY_SIZE(omap2_dmtimer_latency), 1);
	if (IS_ERR(od)) {
		pr_err("%s: Cant build omap_device for %s:%s.\n",
			__func__, name, oh->name);
		kfree(pdata);
	}
	early_timer_count++;
	return 0;
}

static int __init omap2_timer_init(struct omap_hwmod *oh, void *user)
{
	int id;
	char *name = "dmtimer";
	struct omap_device *od;
	struct omap_dmtimer_platform_data *pdata;

	if (!oh) {
		pr_err("%s:NULL hwmod pointer (oh)\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s:%s\n", __func__, oh->name);

	pdata = kzalloc(sizeof(struct omap_dmtimer_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s:No memory for [%s]\n",  __func__, oh->name);
		return -ENOMEM;
	}
	/* hook clock set/get functions */
	pdata->omap_dm_set_source_clk = omap2_dm_timer_set_clk;
	pdata->omap_dm_get_timer_clk = omap2_dm_timer_get_fclk;


	/* Update Highlander offsets for GPTimer [3-9, 11-12].
	 * The offset1 & offset2 will be zero on OMAP3 and
	 * OMAP4 millisecond timers (GPT1, GPT2, GPT10).
	 */
	pdata->timer_ip_type = oh->class->rev;
	if (unlikely(pdata->timer_ip_type == OMAP_TIMER_IP_VERSION_1)) {
		pdata->offset1 = 0x0;
		pdata->offset2 = 0x0;
	} else {
		pdata->offset1 = 0x10;
		pdata->offset2 = 0x14;
	}

	pdata->is_early_init = 0;

	/*
	* extract the id from name
	* this is not the best implementation, but the cleanest with
	* the existing constraints: (1) early timers are not sequential,
	* 1/2/10 (2) export APIs use id's to identify corresponding
	* timers (3) non-early timers initialization have to track the
	* id's already used by early timers.
	*
	* well, the ideal solution would have been to have an 'id' field
	* in struct omap_hwmod {}. otherwise we have to have devattr for
	* all timers.
	*/
	sscanf(oh->name, "timer%2d", &id);

	od = omap_device_build(name, id - 1, oh,
			pdata, sizeof(*pdata),
			omap2_dmtimer_latency,
			ARRAY_SIZE(omap2_dmtimer_latency), 0);
	WARN(IS_ERR(od), "Cant build omap_device for %s:%s.\n",
			name, oh->name);

	return 0;
}

void __init omap2_dm_timer_early_init(void)
{
	if (omap_hwmod_for_each_by_class("timer",
		omap2_timer_early_init, NULL)) {
		pr_debug("%s: device registration FAILED\n", __func__);
		return;
	}
	omap2_dm_timer_setup();
	early_platform_driver_register_all("earlytimer");
	early_platform_driver_probe("earlytimer", early_timer_count + 1, 0);
}

static int __init omap_timer_init(void)
{
	/* register all timers again */
	int ret = omap_hwmod_for_each_by_class("timer", omap2_timer_init, NULL);

	if (unlikely(ret))
		pr_debug("%s: device registration FAILED\n", __func__);

	return ret;
}
arch_initcall(omap_timer_init);
