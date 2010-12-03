/**
 * OMAP2PLUS Dual-Mode Timers - platform device registration
 *
 * Contains first level initialization routines which extracts timers
 * information from hwmod database and registers with linux device model.
 * It also has low level function to change the timer input clock source.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 * Tarun Kanti DebBarma <tarun.kanti@ti.com>
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

static int early_timer_count __initdata = 1;

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
	struct omap_dmtimer_platform_data *pdata = pdev->dev.platform_data;

	if (IS_ERR(timer_clk)) {
		dev_warn(&pdev->dev, "%s: Not able get the clock pointer\n",
			__func__);
		return -EINVAL;
	}

#ifndef CONFIG_PM_RUNTIME
	clk_disable(timer_clk); /* enabled in hwmod */
#else
	if (unlikely(pdata->is_early_init))
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
	if (unlikely(pdata->is_early_init))
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

/**
 * omap_timer_init - build and register timer device with an
 * associated timer hwmod
 * @oh:	timer hwmod pointer to be used to build timer device
 * @user:	parameter that can be passed from calling hwmod API
 *
 * Called by omap_hwmod_for_each_by_class to register each of the timer
 * devices present in the system. The number of timer devices is known
 * by parsing through the hwmod database for a given class name. At the
 * end of function call memory is allocated for timer device and it is
 * registered to the framework ready to be proved by the driver.
 */
static int __init omap_timer_init(struct omap_hwmod *oh, void *user)
{
	int id;
	int ret = 0;
	char *name = "dmtimer";
	struct omap_dmtimer_platform_data *pdata;
	struct omap_device *od;

	pr_debug("%s:%s\n", __func__, oh->name);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: No memory for [%s]\n", __func__, oh->name);
		return -ENOMEM;
	}
	if ((void *)user) {
		pdata->is_early_init = 1;
		pdata->omap_dm_set_source_clk = omap2_dm_early_timer_set_clk;
		pdata->omap_dm_get_timer_clk = omap2_dm_early_timer_get_fclk;
	} else {
		pdata->is_early_init = 0;
		pdata->omap_dm_set_source_clk = omap2_dm_timer_set_clk;
		pdata->omap_dm_get_timer_clk = omap2_dm_timer_get_fclk;
	}

	pdata->timer_ip_type = oh->class->rev;

	if (pdata->timer_ip_type == OMAP_TIMER_IP_LEGACY) {
		pdata->offset1 = 0;
		pdata->offset2 = 0;
	} else {
		pdata->offset1 = 0x10;
		pdata->offset2 = 0x14;
	}

	pdata->is_early_init = 1;

	/*
	 * Extract the id from name field in hwmod database
	 * and use the same for constructing ids' for the
	 * timer devices. In a way, we are avoiding usage of
	 * static variable witin the function to do the same.
	 * CAUTION: We have to be careful and make sure the
	 * name in hwmod database does not change in which case
	 * we might either make corresponding change here or
	 * switch back static variable mechanism.
	 */
	sscanf(oh->name, "timer%2d", &id);

	od = omap_device_build(name, id - 1, oh, pdata, sizeof(*pdata),
			omap2_dmtimer_latency,
			ARRAY_SIZE(omap2_dmtimer_latency), 1);
	if (IS_ERR(od)) {
		pr_err("%s: Cant build omap_device for %s:%s.\n",
			__func__, name, oh->name);
		ret = -EINVAL;
	} else if (pdata->is_early_init)
		early_timer_count++;

	kfree(pdata);

	return ret;
}

/**
 * omap2_dm_timer_early_init - top level early timer initialization
 * called in the last part of omap2_init_common_hw
 *
 * Uses dedicated hwmod api to parse through hwmod database for
 * given class name and then build and register the timer device.
 * At the end driver is registered and early probe initiated.
 */
void __init omap2_dm_timer_early_init(void)
{
	int early_init = 1; /* identify early init in omap2_timer_init() */
	int ret = omap_hwmod_for_each_by_class("timer_1ms",
		omap_timer_init, &early_init);

	omap2_dm_timer_setup();

	if (unlikely(ret)) {
		pr_debug("%s: device registration FAILED!\n", __func__);
		return;
	}

	early_platform_driver_register_all("earlytimer");
	early_platform_driver_probe("earlytimer", early_timer_count, 0);
}

/**
 * omap2_dm_timer_init - top level timer normal device initialization
 *
 * Uses dedicated hwmod API to parse through hwmod database for
 * given class names and then build and register the timer device.
 */
static int __init omap2_dm_timer_init(void)
{
	int ret = omap_hwmod_for_each_by_class("timer", omap_timer_init, NULL);

	if (unlikely(ret))
		pr_debug("%s: device registration FAILED!\n", __func__);

	return ret;
}
arch_initcall(omap2_dm_timer_init);
