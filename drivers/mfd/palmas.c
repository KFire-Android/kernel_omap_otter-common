/*
 * TI Palmas MFD Driver
 *
 * Copyright 2011 Texas Instruments Inc.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/mfd/core.h>
#include <linux/mfd/palmas.h>

#include <linux/i2c/twl-rtc.h>

static struct resource gpadc_resource[] = {
	{
		.name = "EOC_SW",
		.start = PALMAS_GPADC_EOC_SW_IRQ,
		.end = PALMAS_GPADC_EOC_SW_IRQ,
		.flags = IORESOURCE_IRQ,
	}
};

static struct resource usb_resource[] = {
	{
		.name = "ID",
		.start = PALMAS_ID_OTG_IRQ,
		.end = PALMAS_ID_OTG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "ID_WAKEUP",
		.start = PALMAS_ID_IRQ,
		.end = PALMAS_ID_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "VBUS",
		.start = PALMAS_VBUS_OTG_IRQ,
		.end = PALMAS_VBUS_OTG_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.name = "VBUS_WAKEUP",
		.start = PALMAS_VBUS_IRQ,
		.end = PALMAS_VBUS_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource rtc_resource[] = {
	{
		.name = "RTC_ALARM",
		.start = PALMAS_RTC_ALARM_IRQ,
		.end = PALMAS_RTC_ALARM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct resource pwron_resource[] = {
	{
		.name = "PWRON_BUTTON",
		.start = PALMAS_PWRON_IRQ,
		.end = PALMAS_PWRON_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static struct mfd_cell palmas_children[] = {
	{
		.name = "palmas-gpadc",
		.num_resources = ARRAY_SIZE(gpadc_resource),
		.resources = gpadc_resource,
	},
	{
		.name = "palmas-pmic",
		.id = 1,
	},
	{
		.name = "palmas-gpio",
		.id = 2,
	},
	{
		.name = "palmas-resource",
		.id = 3,
	},
	{
		.name = "palmas-clk",
		.id = 4,
	},
	{
		.name = "palmas-leds",
		.id = 5,
	},
	{
		.name = "palmas-wdt",
		.id = 6,
	},
	{
		.name = "palmas-pwm",
		.id = 7,
	},
	{
		.name = "palmas-pwrbutton",
		.id = 8,
		.num_resources = ARRAY_SIZE(pwron_resource),
		.resources = pwron_resource,
	},
	{
		.name = "palmas-usb",
		.num_resources = ARRAY_SIZE(usb_resource),
		.resources = usb_resource,
		.id = 9,
	},
	{
		.name = "twl_rtc",
		.num_resources = ARRAY_SIZE(rtc_resource),
		.resources = rtc_resource,
		.id = 10,
	},
};

static int palmas_rtc_read_block(void *mfd, u8 *dest, u8 reg, int no_regs)
{
	struct palmas *palmas = mfd;
	int slave;
	unsigned int addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_RTC_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_RTC_BASE, reg);

	return regmap_bulk_read(palmas->regmap[slave], addr, dest, no_regs);
}

static int palmas_rtc_write_block(void *mfd, u8 *data, u8 reg, int no_regs)
{
	struct palmas *palmas = mfd;
	int slave;
	unsigned int addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_RTC_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_RTC_BASE, reg);

	/* twl added extra byte on beginning of block data */
	if (no_regs > 1)
		data++;

	return regmap_raw_write(palmas->regmap[slave], addr, data, no_regs);
}

static int palmas_rtc_init(struct palmas *palmas)
{
	struct twl_rtc_data *twl_rtc;

	twl_rtc = kzalloc(sizeof(*twl_rtc), GFP_KERNEL);

	twl_rtc->read_block = palmas_rtc_read_block;
	twl_rtc->write_block = palmas_rtc_write_block;
	twl_rtc->mfd = palmas;
	twl_rtc->chip_version = 6035;

	palmas_children[10].platform_data = twl_rtc;
	palmas_children[10].pdata_size = sizeof(*twl_rtc);

	return 0;
}

static struct regmap_config palmas_regmap_config[PALMAS_NUM_CLIENTS] = {
	{
		.reg_bits = 8,
		.val_bits = 8,
		.cache_type = REGCACHE_NONE,
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.cache_type = REGCACHE_NONE,
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.cache_type = REGCACHE_NONE,
	},
};

static int __devinit palmas_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct palmas *palmas;
	struct palmas_platform_data *mfd_platform_data;
	int ret = 0, i;
	unsigned int reg, addr;
	int slave;
	char *rname;

	mfd_platform_data = dev_get_platdata(&i2c->dev);
	if (!mfd_platform_data)
		return -EINVAL;

	palmas = kzalloc(sizeof(struct palmas), GFP_KERNEL);
	if (palmas == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, palmas);
	palmas->dev = &i2c->dev;
	palmas->id = id->driver_data;

	ret = irq_alloc_descs(-1, 0, PALMAS_NUM_IRQ, 0);
	if (ret < 0) {
		dev_err(&i2c->dev, "failed to allocate IRQ descs\n");
		goto err;
	}

	palmas->irq = i2c->irq;
	palmas->irq_base = ret;
	palmas->irq_end = ret + PALMAS_NUM_IRQ;

	for (i = 0; i < PALMAS_NUM_CLIENTS; i++) {
		if (i == 0)
			palmas->i2c_clients[i] = i2c;
		else {
			palmas->i2c_clients[i] =
					i2c_new_dummy(i2c->adapter,
							i2c->addr + i);
			if (!palmas->i2c_clients[i]) {
				dev_err(palmas->dev,
					"can't attach client %d\n", i);
				ret = -ENOMEM;
				goto err;
			}
		}
		palmas->regmap[i] = regmap_init_i2c(palmas->i2c_clients[i],
				&palmas_regmap_config[i]);
		if (IS_ERR(palmas->regmap[i])) {
			ret = PTR_ERR(palmas->regmap[i]);
			dev_err(palmas->dev, "Failed to allocate register map "
					"No: %d, because: %d\n", i, ret);
			goto err;
		}
	}

	ret = palmas_irq_init(palmas);
	if (ret < 0)
		goto err;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_DESIGNREV_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_DESIGNREV_BASE, 0);
	/*
	 * Revision either
	 * PALMAS_REV_ES1_0 or
	 * PALMAS_REV_ES2_0 or
	 * PALMAS_REV_ES2_1
	 */
	ret = regmap_read(palmas->regmap[slave], addr, &reg);
	if (ret)
		goto err;

	palmas->revision = reg;
	switch (palmas->revision) {
	case PALMAS_REV_ES1_0:
		rname = "ES 1.0";
		break;
	case PALMAS_REV_ES2_0:
		rname = "ES 2.0";
		break;
	case PALMAS_REV_ES2_1:
		rname = "ES 2.1";
		break;
	default:
		rname = "unknown";
		break;
	}
	dev_info(palmas->dev, "%s %s detected\n", id->name, rname);

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PU_PD_OD_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD1);

	if (mfd_platform_data->mux_from_pdata) {
		reg = mfd_platform_data->pad1;
		ret = regmap_write(palmas->regmap[slave], addr, reg);
		if (ret)
			goto err;
	} else {
		ret = regmap_read(palmas->regmap[slave], addr, &reg);
		if (ret)
			goto err;
	}

	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_0))
		palmas->gpio_muxed |= PALMAS_GPIO_0_MUXED;
	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_1_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_1_MUXED;
	else if ((reg & PRIMARY_SECONDARY_PAD1_GPIO_1_MASK) ==
			(2 << PRIMARY_SECONDARY_PAD1_GPIO_1_SHIFT))
		palmas->led_muxed |= PALMAS_LED1_MUXED;
	else if ((reg & PRIMARY_SECONDARY_PAD1_GPIO_1_MASK) ==
			(3 << PRIMARY_SECONDARY_PAD1_GPIO_1_SHIFT))
		palmas->pwm_muxed |= PALMAS_PWM1_MUXED;
	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_2_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_2_MUXED;
	else if ((reg & PRIMARY_SECONDARY_PAD1_GPIO_2_MASK) ==
			(2 << PRIMARY_SECONDARY_PAD1_GPIO_2_SHIFT))
		palmas->led_muxed |= PALMAS_LED2_MUXED;
	else if ((reg & PRIMARY_SECONDARY_PAD1_GPIO_2_MASK) ==
			(3 << PRIMARY_SECONDARY_PAD1_GPIO_2_SHIFT))
		palmas->pwm_muxed |= PALMAS_PWM2_MUXED;
	if (!(reg & PRIMARY_SECONDARY_PAD1_GPIO_3))
		palmas->gpio_muxed |= PALMAS_GPIO_3_MUXED;

	addr = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD2);

	if (mfd_platform_data->mux_from_pdata) {
		reg = mfd_platform_data->pad2;
		ret = regmap_write(palmas->regmap[slave], addr, reg);
		if (ret)
			goto err;
	} else {
		ret = regmap_read(palmas->regmap[slave], addr, &reg);
		if (ret)
			goto err;
	}

	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_4))
		palmas->gpio_muxed |= PALMAS_GPIO_4_MUXED;
	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_5_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_5_MUXED;
	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_6))
		palmas->gpio_muxed |= PALMAS_GPIO_6_MUXED;
	if (!(reg & PRIMARY_SECONDARY_PAD2_GPIO_7_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_7_MUXED;

	dev_info(palmas->dev, "Muxing GPIO %x, PWM %x, LED %x\n",
			palmas->gpio_muxed, palmas->pwm_muxed,
			palmas->led_muxed);

	reg = mfd_platform_data->power_ctrl;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PMU_CONTROL_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE, PALMAS_POWER_CTRL);

	ret = regmap_write(palmas->regmap[slave], addr, reg);
	if (ret)
		goto err;

	palmas_rtc_init(palmas);

	ret = mfd_add_devices(palmas->dev, -1,
			      palmas_children, ARRAY_SIZE(palmas_children),
			      NULL, palmas->irq_base);
	if (ret < 0)
		goto err;

	return ret;

err:
	mfd_remove_devices(palmas->dev);
	kfree(palmas);
	return ret;
}

static int palmas_i2c_remove(struct i2c_client *i2c)
{
	struct palmas *palmas = i2c_get_clientdata(i2c);

	mfd_remove_devices(palmas->dev);
	palmas_irq_exit(palmas);
	kfree(palmas);

	return 0;
}

static const struct i2c_device_id palmas_i2c_id[] = {
	{ "twl6035", PALMAS_ID_TWL6035 },
	{ "tps65913", PALMAS_ID_TPS65913 },
};
MODULE_DEVICE_TABLE(i2c, palmas_i2c_id);

static struct of_device_id __devinitdata of_palmas_match_tbl[] = {
	{ .compatible = "ti,twl6035", },
	{ .compatible = "ti,tps65913", },
	{ /* end */ }
};

static struct i2c_driver palmas_i2c_driver = {
	.driver = {
		   .name = "palmas",
		   .of_match_table = of_palmas_match_tbl,
		   .owner = THIS_MODULE,
	},
	.probe = palmas_i2c_probe,
	.remove = palmas_i2c_remove,
	.id_table = palmas_i2c_id,
};

static int __init palmas_i2c_init(void)
{
	return i2c_add_driver(&palmas_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(palmas_i2c_init);

static void __exit palmas_i2c_exit(void)
{
	i2c_del_driver(&palmas_i2c_driver);
}
module_exit(palmas_i2c_exit);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas chip family multi-function driver");
MODULE_LICENSE("GPL");
