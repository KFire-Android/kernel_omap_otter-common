/*
 * arch/arm/mach-omap2/omap-rfkill-ste.c
 *
 * OMAP driver file for rfkill ST-E modem driver
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <plat/omap_hwmod.h>
#include "mux.h"
#include "omap-rfkill-ste.h"


enum {
	RFKILL_STE_P57XX,
	RFKILL_STE_G74XX,
};

struct omap_rfkill_ste_data {
	struct rfkill			*rfkdev;
	struct omap_hwmod_mux_info	*omap_pads;
};


static int omap_rfkill_ste_set_block(void *data, bool blocked)
{
	struct platform_device *pdev = data;
	dev_info(&pdev->dev, "%s: %d\n", __func__, blocked);

	return 0;
}


static const struct rfkill_ops omap_rfkill_ste_ops = {
	.set_block = omap_rfkill_ste_set_block,
};


static int omap_rfkill_ste_probe(struct platform_device *pdev)
{
	struct omap_rfkill_ste_data *rfkill;
	struct omap_rfkill_ste_platform_data *pdata
						= dev_get_platdata(&pdev->dev);
	struct rfkill *rfkdev;
	int ret = 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	if (!pdata) {
		dev_err(&pdev->dev,
			"%s: no platform data specified\n", __func__);
		ret = -EINVAL;
		goto error0;
	}

	rfkill = kzalloc(sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill) {
		dev_err(&pdev->dev,
			"%s: no memory to alloc driver data\n", __func__);
		ret = -ENOMEM;
		goto error0;
	}

	platform_set_drvdata(pdev, rfkill);

	if (pdata->omap_pads)
		rfkill->omap_pads = omap_hwmod_mux_init(pdata->omap_pads,
							pdata->omap_pads_len);

	/* WWAN rfkill device registration */
	rfkill->rfkdev = rfkill_alloc(pdev->name,
					&pdev->dev,
					RFKILL_TYPE_WWAN,
					&omap_rfkill_ste_ops,
					pdev);
	rfkdev = rfkill->rfkdev;
	if (!rfkdev) {
		dev_err(&pdev->dev,
			"%s: Error allocating modem rfkdev\n", __func__);
		ret = -ENOMEM;
		goto error1;
	}

	/* S/W blocked by default, persistent */
	rfkill_init_sw_state(rfkdev, 1);
	ret = rfkill_register(rfkdev);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Error registering modem rfkdev: %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto error2;
	}

	/* hardware unblocked */
	if (rfkill->rfkdev)
		rfkill_set_hw_state(rfkdev, 0);

	return 0;

error2:
	rfkill_destroy(rfkdev);
error1:
	kfree(rfkill);
error0:
	return ret;
}


static int omap_rfkill_ste_remove(struct platform_device *pdev)
{
	struct omap_rfkill_ste_data *rfkill = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill_unregister(rfkill->rfkdev);
	rfkill_destroy(rfkill->rfkdev);
	kfree(rfkill);

	return 0;
}


/* List of device names supported by this driver */
static struct platform_device_id omap_rfkill_ste_id_table[] = {
	{"ste-p5780", RFKILL_STE_P57XX},
	{"ste-g7400", RFKILL_STE_G74XX},
	{},
};
MODULE_DEVICE_TABLE(platform, omap_rfkill_ste_id_table);

static struct platform_driver omap_rfkill_ste_driver = {
	.probe = omap_rfkill_ste_probe,
	.remove = __devexit_p(omap_rfkill_ste_remove),
	.driver = {
		.name = "omap_ste_rfkill",
		.owner = THIS_MODULE,
	},
	.id_table = omap_rfkill_ste_id_table,
};


static int __init omap_rfkill_ste_init(void)
{
	pr_debug("%s\n", __func__);

	return platform_driver_register(&omap_rfkill_ste_driver);
}

static void __exit omap_rfkill_ste_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&omap_rfkill_ste_driver);
}

module_init(omap_rfkill_ste_init);
module_exit(omap_rfkill_ste_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments Inc");
MODULE_DESCRIPTION("OMAP RFKILL driver for ST-E modem");

