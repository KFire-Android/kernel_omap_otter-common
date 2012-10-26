/*
 * Palmas s/w power OFF support
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/mfd/palmas.h>

/* One instance of palmas supported */
static struct palmas *my_palmas;

/*
 * Allow to differentiate between MFD removal as part of shutdown
 * and driver removal
 */
static bool palmas_driver_removal;

/**
 * palmas_poweroff_write() - write to palmas register
 * @base:	module base
 * @reg:	register
 * @mask:	mask to use
 * @val:	value to write
 */
static int palmas_poweroff_write(unsigned int base, unsigned int reg,
				 unsigned int mask, unsigned int val)
{
	struct palmas *palmas = my_palmas;
	unsigned int addr;
	int slave;
	int r;

	slave = PALMAS_BASE_TO_SLAVE(base);
	addr = PALMAS_BASE_TO_REG(base, reg);

	r = regmap_update_bits(palmas->regmap[slave], addr, mask, val);

	return r;
}

/**
 * palmas_poweroff() - power off palmas
 *
 * Power OFF palmas, but allow VBUS to wakeup Palmas to allow
 * basic things like charging, flashing etc.
 */
static void palmas_poweroff(void)
{
	int ret;

	/* At least 1 palmas device needed to shutdown */
	if (!my_palmas) {
		pr_err("%s: NO PALMAS device registered!\n", __func__);
		goto out;
	}

	/* Unmask VBUSIRQ so that USB power can wakeup device after shutdown */
	ret = palmas_poweroff_write(PALMAS_INTERRUPT_BASE,
				    PALMAS_INT3_MASK, INT3_MASK_VBUS, 0x0);
	if (ret)
		pr_err("%s: Unable to clear VBUS in INT3_MASK: %d\n",
		       __func__, ret);

	/* Now, shutdown palmas */
	ret = palmas_poweroff_write(PALMAS_PMU_CONTROL_BASE,
				    PALMAS_DEV_CTRL,
				    DEV_CTRL_SW_RST | DEV_CTRL_DEV_ON,
				    0x0);
	if (ret)
		pr_err("%s: Unable to write to DEV_CTRL_SW_RST: %d\n",
		       __func__, ret);

	/* arbitary 100ms delay to ensure shutdown */
	mdelay(100);
out:
	/* Hope the reset takes care of things */
	pr_err("%s: Palmas did not shutdown!!!\n", __func__);
	BUG();
	return;
}

static int __devinit palmas_poweroff_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	int ret = -EINVAL;

	/* Only one palmas can be allowed by poweroff */
	if (my_palmas)
		goto out;

	my_palmas = palmas;

	/* Keep device in active mode */
	ret = palmas_poweroff_write(PALMAS_PMU_CONTROL_BASE,
				    PALMAS_DEV_CTRL,
				    DEV_CTRL_DEV_ON, DEV_CTRL_DEV_ON);
	if (ret)
		pr_err("%s: Unable to write to DEV_CTRL_DEV_ON: %d\n",
		       __func__, ret);
	/* Fall through */
	if (ret)
		my_palmas = NULL;
	else
		pm_power_off = palmas_poweroff;

out:
	return ret;
}

static int __devexit palmas_poweroff_remove(struct platform_device *pdev)
{
	/*
	 * If my module is not really getting removed, dont reset ptrs yet.
	 * we *do* want the handlers to exist to allow system to shutdown
	 */
	if (!palmas_driver_removal)
		return 0;

	pm_power_off = NULL;
	my_palmas = NULL;

	return 0;
}

static struct platform_driver palmas_poweroff_driver = {
	.probe = palmas_poweroff_probe,
	.remove = __devexit_p(palmas_poweroff_remove),
	.driver = {
		   .name = "palmas-poweroff",
		   .owner = THIS_MODULE,
		   },
};

static int __init palmas_poweroff_init(void)
{
	return platform_driver_register(&palmas_poweroff_driver);
}
module_init(palmas_poweroff_init);

static void __exit palmas_poweroff_exit(void)
{
	palmas_driver_removal = true;
	platform_driver_unregister(&palmas_poweroff_driver);
}
module_exit(palmas_poweroff_exit);

MODULE_AUTHOR("Nishanth Menon <nm@ti.com>");
MODULE_DESCRIPTION("Palmas poweroff driver");
MODULE_ALIAS("platform:palmas-poweroff");
MODULE_LICENSE("GPL");
