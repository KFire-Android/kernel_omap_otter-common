/*
 * gpio.c - OMAP2PLUS-specific gpio code
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Author:
 *	Charulatha V <charu@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>

/*
 * In interrupt disabled context, the following wrapper is required
 * as the runtime PM FW does not have mutex-free functions.
 */ 
static int gpio_enable_hwmod(struct omap_device *od)
{
	struct omap_hwmod *oh = *od->hwmods;

	if (irqs_disabled())
		_omap_hwmod_enable(oh);
	else
		omap_device_enable_hwmods(od);
	return 0;
}

static int gpio_idle_hwmod(struct omap_device *od)
{
	struct omap_hwmod *oh = *od->hwmods;

	if (irqs_disabled())
		_omap_hwmod_idle(oh);
	else
		omap_device_idle_hwmods(od);
	return 0;
}

static struct omap_device_pm_latency omap_gpio_latency[] = {
	[0] = {
		.deactivate_func = gpio_idle_hwmod,
		.activate_func   = gpio_enable_hwmod,
		.flags		 = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int omap2_init_gpio(struct omap_hwmod *oh, void *user)
{
	struct omap_device *od;
	struct omap_gpio_platform_data *pdata;
	char *name = "omap-gpio";
	static int id;
	struct omap_gpio_dev_attr *gpio_dev_data;

	if (!oh) {
		pr_err("Could not look up omap gpio %d\n", id + 1);
		return -EFAULT;
	}

	pdata = kzalloc(sizeof(struct omap_gpio_platform_data),
					GFP_KERNEL);
	if (!pdata) {
		pr_err("Memory allocation failed gpio%d\n", id + 1);
		return -ENOMEM;
	}

	gpio_dev_data = (struct omap_gpio_dev_attr *)oh->dev_attr;

	pdata->gpio_attr = gpio_dev_data;
	pdata->virtual_irq_start = IH_GPIO_BASE + 32 * id;
	switch (oh->class->rev) {
	case 0:
	case 1:
		pdata->bank_type = METHOD_GPIO_24XX;
		break;
	case 2:
		pdata->bank_type = METHOD_GPIO_44XX;
		break;
	default:
		WARN(1, "Invalid gpio bank_type\n");
		break;
	}
	gpio_bank_count++;

	od = omap_device_build(name, id, oh, pdata,
				sizeof(*pdata),	omap_gpio_latency,
				ARRAY_SIZE(omap_gpio_latency),
				false);
	WARN(IS_ERR(od), "Cant build omap_device for %s:%s.\n",
				name, oh->name);

	id++;
	return 0;
}

/*
 * gpio_init needs to be done before
 * machine_init functions access gpio APIs.
 * Hence gpio_init is a postcore_initcall.
 */
static int __init omap2_gpio_init(void)
{
	return omap_hwmod_for_each_by_class("gpio", omap2_init_gpio,
						NULL);
}
postcore_initcall(omap2_gpio_init);
