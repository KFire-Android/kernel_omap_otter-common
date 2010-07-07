/*
 * OMAP4 EMIF platform driver
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Santosh Shilimkar (santosh.shilimkar@ti.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <mach/emif-44xx.h>
#include <mach/dmm-44xx.h>


struct emif_instance {
	void __iomem *base;
	u16 irq;
};
static struct emif_instance emif[2];

struct omap_device_pm_latency omap_emif_latency[] = {
	[0] = {
		.deactivate_func	= omap_device_idle_hwmods,
		.activate_func		= omap_device_enable_hwmods,
		.flags			= OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

/*
 * Interrupt Handler for EMIF1 and EMIF2
 */
static irqreturn_t emif_interrupt_handler(int irq, void *dev_id)
{
	/*
	 * TBD: Thermal management
	 */

	return IRQ_HANDLED;
}

static void __init setup_emif_interrupts(void __iomem *base)
{
	/*
	 * TBD: Thermal management:
	 * Setup the temperature alret interrupt
	 * Enable the temparature alert interrupt
	 */

	/*
	 * Clear any pendining interrupts
	 */
	__raw_writel(0x0, base + OMAP44XX_EMIF_IRQENABLE_CLR_SYS);
	__raw_writel(0x0, base + OMAP44XX_EMIF_IRQENABLE_CLR_LL);
}

static int __devinit omap_emif_probe(struct platform_device *pdev)
{
	int id;
	int ret;
	struct resource *res;

	if (!pdev)
		return -EINVAL;

	id = pdev->id;
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("EMIF %i Invalid IRQ resource\n", id);
		return -ENODEV;
	}

	emif[id].irq = res->start;
	ret = request_irq(emif[id].irq,
			(irq_handler_t)emif_interrupt_handler,
			IRQF_SHARED, pdev->name, pdev);
	if (ret) {
		pr_err("request_irq failed to register for 0x%x\n",
					 emif[id].irq);
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("EMIF%i Invalid mem resource\n", id);
		return -ENODEV;
	}

	emif[id].base = ioremap(res->start, SZ_1M);
	if (!emif[id].base) {
		pr_err("Could not ioremap EMIF%i\n", id);
		return -ENOMEM;
	}

	/*
	 * EMIF devices are enabled by the hwmod
	 * framework. Setup the IRQs
	 */
	setup_emif_interrupts(emif[id].base);


	pr_info("EMIF%d is enabled with IRQ%d\n", id, emif[id].irq);

	return 0;
}

static int emif_init(struct omap_hwmod *oh, void *user)
{
	char *name = "omap_emif";
	struct omap_device *od;
	static int id;

	od = omap_device_build(name, id, oh, NULL, 0, omap_emif_latency,
				ARRAY_SIZE(omap_emif_latency), false);
	WARN(IS_ERR(od), "Can't build omap_device for %s:%s.\n",
				name, oh->name);
	id++;
	return 0;

}

/*
 * omap_emif_device_init needs to be done before
 * ddr reconfigure function call.
 * Hence omap_emif_device_init is a postcore_initcall.
 */
static int __init omap_emif_device_init(void)
{
	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!cpu_is_omap44xx())
		return -ENODEV;

	return omap_hwmod_for_each_by_class("emif", emif_init, NULL);
}
postcore_initcall(omap_emif_device_init);

static struct platform_driver omap_emif_driver = {
	.probe		= omap_emif_probe,
	.driver		= {
		.name	= "omap_emif",
	},
};

static int __init omap_emif_register(void)
{
	return platform_driver_register(&omap_emif_driver);
}
postcore_initcall(omap_emif_register);
