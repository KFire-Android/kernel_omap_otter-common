/*
 * drivers/net/wireless/ste-rfkill.c
 *
 * Driver file for ST-E rfkill driver
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


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/rfkill.h>


/*------------------- RFKILL DRIVER DEVICE DATA STRUCTURE -------------------*/

struct ste_rfkill_data {
	struct rfkill			*rfkdev;
};


/*------------------------------ RFKILL DRIVER ------------------------------*/

enum {
	RFKILL_STE_P57XX,
	RFKILL_STE_G74XX,
};


static int ste_rfkill_set_block(void *data, bool blocked)
{
	struct platform_device *pdev = data;
	dev_info(&pdev->dev, "%s: %d\n", __func__, blocked);

	return 0;
}


static const struct rfkill_ops ste_rfkill_ops = {
	.set_block = ste_rfkill_set_block,
};


static int ste_rfkill_probe(struct platform_device *pdev)
{
	struct ste_rfkill_data *rfkill;
	struct rfkill *rfkdev;
	int ret = 0;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill = kzalloc(sizeof(*rfkill), GFP_KERNEL);
	if (!rfkill) {
		dev_err(&pdev->dev,
			"%s: no memory to alloc driver data\n", __func__);
		ret = -ENOMEM;
		goto error0;
	}

	platform_set_drvdata(pdev, rfkill);

	/* WWAN rfkill device registration */
	rfkill->rfkdev = rfkill_alloc(pdev->name,
					&pdev->dev,
					RFKILL_TYPE_WWAN,
					&ste_rfkill_ops,
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


static int ste_rfkill_remove(struct platform_device *pdev)
{
	struct ste_rfkill_data *rfkill = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill_unregister(rfkill->rfkdev);
	rfkill_destroy(rfkill->rfkdev);
	kfree(rfkill);

	return 0;
}


/* List of device names supported by this driver */
static struct platform_device_id ste_rfkill_id_table[] = {
	{"ste-p5780", RFKILL_STE_P57XX},
	{"ste-g7400", RFKILL_STE_G74XX},
	{},
};
MODULE_DEVICE_TABLE(platform, ste_rfkill_id_table);

static struct platform_driver ste_rfkill_driver = {
	.probe = ste_rfkill_probe,
	.remove = __devexit_p(ste_rfkill_remove),
	.driver = {
		.name = "ste_rfkill",
		.owner = THIS_MODULE,
	},
	.id_table = ste_rfkill_id_table,
};


static int __init ste_rfkill_init(void)
{
	pr_debug("%s\n", __func__);

	return platform_driver_register(&ste_rfkill_driver);
}

static void __exit ste_rfkill_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&ste_rfkill_driver);
}

module_init(ste_rfkill_init);
module_exit(ste_rfkill_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Texas Instruments Inc");
MODULE_DESCRIPTION("RFKILL driver for ST-E modem");
