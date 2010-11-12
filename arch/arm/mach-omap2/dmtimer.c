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
#undef DEBUG

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

/**
 * omap2_dm_timer_set_src - change the timer input clock source
 * @pdev:	timer platform device pointer
 * @timer_clk:	current clock source
 * @source:	array index of parent clock source
 */
static int omap2_dm_timer_set_src(struct platform_device *pdev, int source)
{
	int ret;
	struct dmtimer_platform_data *pdata = pdev->dev.platform_data;
	struct clk *fclk = clk_get(&pdev->dev, "fck");
	struct clk *new_fclk;
	char *fclk_name = "32k_ck"; /* default name */

	switch (source) {
	case OMAP_TIMER_SRC_SYS_CLK:
		fclk_name = "sys_ck";
		break;

	case OMAP_TIMER_SRC_32_KHZ:
		fclk_name = "32k_ck";
		break;

	case OMAP_TIMER_SRC_EXT_CLK:
		if (pdata->timer_ip_type == OMAP_TIMER_IP_VERSION_1) {
			fclk_name = "alt_ck";
			break;
		}
		dev_dbg(&pdev->dev, "%s:%d: invalid clk src.\n",
			__func__, __LINE__);
		return -EINVAL;
	}


	if (IS_ERR_OR_NULL(fclk)) {
		dev_dbg(&pdev->dev, "%s:%d: clk_get() FAILED\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	new_fclk = clk_get(&pdev->dev, fclk_name);
	if (IS_ERR_OR_NULL(new_fclk)) {
		dev_dbg(&pdev->dev, "%s:%d: clk_get() %s FAILED\n",
			__func__, __LINE__, fclk_name);
		clk_put(fclk);
		return -EINVAL;
	}

	ret = clk_set_parent(fclk, new_fclk);
	clk_put(new_fclk);
	clk_put(fclk);
	if (IS_ERR_VALUE(ret)) {
		dev_dbg(&pdev->dev, "%s:clk_set_parent() to %s FAILED\n",
			__func__, fclk_name);
		return -EINVAL;
	}

	return 0;
}

struct omap_device_pm_latency omap2_dmtimer_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

/**
 * omap2_timer_early_init - build and register early timer device
 * with an associated timer hwmod
 * @oh: timer hwmod pointer to be used to build timer device
 * @user: parameter that can be passed from calling hwmod API
 *
 * early init is called in the last part of omap2_init_common_hw
 * for each early timer class using omap_hwmod_for_each_by_class.
 * it registers each of the timer devices present in the system.
 * at the end of function call memory is allocated for omap_device
 * and hwmod for early timer and the device is registered to the
 * framework ready to be probed by the driver.
 */
static int __init omap2_timer_early_init(struct omap_hwmod *oh, void *user)
{
	int id;
	int ret = 0;
	char *name = "omap-timer";
	struct dmtimer_platform_data *pdata;
	struct omap_device *od;

	pr_debug("%s:%s\n", __func__, oh->name);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: No memory for [%s]\n", __func__, oh->name);
		return -ENOMEM;
	}

	pdata->set_timer_src = omap2_dm_timer_set_src;

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
	 * extract the id from name filed in hwmod database
	 * and use the same for constructing ids' for the
	 * timer devices. in a way, we are avoiding usage of
	 * static variable witin the function to do the same.
	 * CAUTION: we have to be careful and make sure the
	 * name in hwmod database does not change in which case
	 * we might either make corresponding change here or
	 * switch back static variable mechanism.
	 */
	sscanf(oh->name, "timer%2d", &id);

	od = omap_device_build(name, id, oh, pdata, sizeof(*pdata),
			omap2_dmtimer_latency,
			ARRAY_SIZE(omap2_dmtimer_latency), 1);

	if (IS_ERR(od)) {
		pr_err("%s: Cant build omap_device for %s:%s.\n",
			__func__, name, oh->name);
		ret = -EINVAL;
	} else
		early_timer_count++;
	/*
	 * pdata can be freed because omap_device_build
	 * creates its own memory pool
	 */
	kfree(pdata);

	return ret;
}

/**
 * omap2_timer_init - build and register timer device with an
 * associated timer hwmod
 * @oh:	timer hwmod pointer to be used to build timer device
 * @user:	parameter that can be passed from calling hwmod API
 *
 * called by omap_hwmod_for_each_by_class to register each of the timer
 * devices present in the system. the number of timer devices is known
 * by parsing through the hwmod database for a given class name. at the
 * end of function call memory is allocated for omap_device and hwmod
 * for timer and the device is registered to the framework ready to be
 * proved by the driver.
 */
static int __init omap2_timer_init(struct omap_hwmod *oh, void *user)
{
	int id;
	int ret = 0;
	char *name = "omap-timer";
	struct omap_device *od;
	struct dmtimer_platform_data *pdata;

	pr_debug("%s:%s\n", __func__, oh->name);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s:No memory for [%s]\n",  __func__, oh->name);
		return -ENOMEM;
	}

	pdata->set_timer_src = omap2_dm_timer_set_src;

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
	 * extract the id from name filed in hwmod database
	 * and use the same for constructing ids' for the
	 * timer devices. in a way, we are avoiding usage of
	 * static variable witin the function to do the same.
	 * CAUTION: we have to be careful and make sure the
	 * name in hwmod database does not change in which case
	 * we might either make corresponding change here or
	 * switch back static variable mechanism.
	 */
	sscanf(oh->name, "timer%2d", &id);

	od = omap_device_build(name, id, oh,
			pdata, sizeof(*pdata),
			omap2_dmtimer_latency,
			ARRAY_SIZE(omap2_dmtimer_latency), 0);

	if (IS_ERR(od)) {
		pr_err("%s: Cant build omap_device for %s:%s.\n",
			__func__, name, oh->name);
		ret =  -EINVAL;
	}
	/*
	 * pdata can be freed because omap_device_build
	 * creates its own memory pool
	 */
	kfree(pdata);
	return ret;
}

/**
 * omap2_dm_timer_early_init - top level early timer initialization
 * called in the last part of omap2_init_common_hw
 *
 * uses dedicated hwmod api to parse through hwmod database for
 * given class name and then build and register the timer device.
 * at the end driver is registered and early probe initiated.
 */
void __init omap2_dm_timer_early_init(void)
{
	if (omap_hwmod_for_each_by_class("timer",
		omap2_timer_early_init, NULL)) {
		pr_debug("%s: device registration FAILED\n", __func__);
		return;
	}
	early_platform_driver_register_all("earlytimer");
	early_platform_driver_probe("earlytimer", early_timer_count, 0);
}

/**
 * omap2_dmtimer_device_init - top level timer device initialization
 *
 * uses dedicated hwmod api to parse through hwmod database for
 * given class names and then build and register the timer device.
 */
static int __init omap2_dmtimer_device_init(void)
{
	int ret = omap_hwmod_for_each_by_class("timer", omap2_timer_init, NULL);

	if (unlikely(ret))
		pr_debug("%s: device registration FAILED\n", __func__);

	return ret;
}
arch_initcall(omap2_dmtimer_device_init);
