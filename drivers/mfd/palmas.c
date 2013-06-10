/*
 * TI Palmas MFD Driver
 *
 * Copyright 2011-2012 Texas Instruments Inc.
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/regmap.h>
#include <linux/err.h>
#include <linux/mfd/core.h>
#include <linux/mfd/palmas.h>
#include <linux/of_platform.h>

enum palmas_ids {
	PALMAS_PMIC_ID,
	PALMAS_GPIO_ID,
	PALMAS_LEDS_ID,
	PALMAS_WDT_ID,
	PALMAS_RTC_ID,
	PALMAS_PWRBUTTON_ID,
	PALMAS_GPADC_ID,
	PALMAS_RESOURCE_ID,
	PALMAS_CLK_ID,
	PALMAS_PWM_ID,
	PALMAS_USB_ID,
};

static struct resource palmas_rtc_resources[] = {
	{
		.start  = PALMAS_RTC_ALARM_IRQ,
		.end    = PALMAS_RTC_ALARM_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static const struct mfd_cell palmas_children[] = {
	{
		.name = "palmas-pmic",
		.id = PALMAS_PMIC_ID,
	},
	{
		.name = "palmas-gpio",
		.id = PALMAS_GPIO_ID,
	},
	{
		.name = "palmas-leds",
		.id = PALMAS_LEDS_ID,
	},
	{
		.name = "palmas-wdt",
		.id = PALMAS_WDT_ID,
	},
	{
		.name = "palmas-rtc",
		.id = PALMAS_RTC_ID,
		.resources = &palmas_rtc_resources[0],
		.num_resources = ARRAY_SIZE(palmas_rtc_resources),
	},
	{
		.name = "palmas-pwrbutton",
		.id = PALMAS_PWRBUTTON_ID,
	},
	{
		.name = "palmas-gpadc",
		.id = PALMAS_GPADC_ID,
	},
	{
		.name = "palmas-resource",
		.id = PALMAS_RESOURCE_ID,
	},
	{
		.name = "palmas-clk",
		.id = PALMAS_CLK_ID,
	},
	{
		.name = "palmas-pwm",
		.id = PALMAS_PWM_ID,
	},
	{
		.name = "palmas-usb",
		.id = PALMAS_USB_ID,
	}
};

static const struct mfd_cell tps659038_children[] = {
	{
		.name = "tps659038-pmic",
		.id = PALMAS_PMIC_ID,
	},
	{
		.name = "tps659038-gpio",
		.id = PALMAS_GPIO_ID,
	},
	{
		.name = "tps659038-leds",
		.id = PALMAS_LEDS_ID,
	},
	{
		.name = "tps659038-wdt",
		.id = PALMAS_WDT_ID,
	},
	{
		.name = "tps659038-rtc",
		.id = PALMAS_RTC_ID,
	},
	{
		.name = "tps659038-pwrbutton",
		.id = PALMAS_PWRBUTTON_ID,
	},
	{
		.name = "tps659038-gpadc",
		.id = PALMAS_GPADC_ID,
	},
	{
		.name = "tps659038-resource",
		.id = PALMAS_RESOURCE_ID,
	},
	{
		.name = "tps659038-clk",
		.id = PALMAS_CLK_ID,
	},
	{
		.name = "tps659038-pwm",
		.id = PALMAS_PWM_ID,
	}
};

static const struct regmap_config palmas_regmap_config[PALMAS_NUM_CLIENTS] = {
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
					PALMAS_PRIMARY_SECONDARY_PAD3),
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_GPADC_BASE,
					PALMAS_GPADC_SMPS_VSEL_MONITORING),
	},
	{
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = PALMAS_BASE_TO_REG(PALMAS_TRIM_GPADC_BASE,
					PALMAS_GPADC_TRIM16),
	},
};

static const struct regmap_irq palmas_irqs[] = {
	/* INT1 IRQs */
	[PALMAS_CHARG_DET_N_VBUS_OVV_IRQ] = {
		.mask = PALMAS_INT1_STATUS_CHARG_DET_N_VBUS_OVV,
	},
	[PALMAS_PWRON_IRQ] = {
		.mask = PALMAS_INT1_STATUS_PWRON,
	},
	[PALMAS_LONG_PRESS_KEY_IRQ] = {
		.mask = PALMAS_INT1_STATUS_LONG_PRESS_KEY,
	},
	[PALMAS_RPWRON_IRQ] = {
		.mask = PALMAS_INT1_STATUS_RPWRON,
	},
	[PALMAS_PWRDOWN_IRQ] = {
		.mask = PALMAS_INT1_STATUS_PWRDOWN,
	},
	[PALMAS_HOTDIE_IRQ] = {
		.mask = PALMAS_INT1_STATUS_HOTDIE,
	},
	[PALMAS_VSYS_MON_IRQ] = {
		.mask = PALMAS_INT1_STATUS_VSYS_MON,
	},
	[PALMAS_VBAT_MON_IRQ] = {
		.mask = PALMAS_INT1_STATUS_VBAT_MON,
	},
	/* INT2 IRQs*/
	[PALMAS_RTC_ALARM_IRQ] = {
		.mask = PALMAS_INT2_STATUS_RTC_ALARM,
		.reg_offset = 1,
	},
	[PALMAS_RTC_TIMER_IRQ] = {
		.mask = PALMAS_INT2_STATUS_RTC_TIMER,
		.reg_offset = 1,
	},
	[PALMAS_WDT_IRQ] = {
		.mask = PALMAS_INT2_STATUS_WDT,
		.reg_offset = 1,
	},
	[PALMAS_BATREMOVAL_IRQ] = {
		.mask = PALMAS_INT2_STATUS_BATREMOVAL,
		.reg_offset = 1,
	},
	[PALMAS_RESET_IN_IRQ] = {
		.mask = PALMAS_INT2_STATUS_RESET_IN,
		.reg_offset = 1,
	},
	[PALMAS_FBI_BB_IRQ] = {
		.mask = PALMAS_INT2_STATUS_FBI_BB,
		.reg_offset = 1,
	},
	[PALMAS_SHORT_IRQ] = {
		.mask = PALMAS_INT2_STATUS_SHORT,
		.reg_offset = 1,
	},
	[PALMAS_VAC_ACOK_IRQ] = {
		.mask = PALMAS_INT2_STATUS_VAC_ACOK,
		.reg_offset = 1,
	},
	/* INT3 IRQs */
	[PALMAS_GPADC_AUTO_0_IRQ] = {
		.mask = PALMAS_INT3_STATUS_GPADC_AUTO_0,
		.reg_offset = 2,
	},
	[PALMAS_GPADC_AUTO_1_IRQ] = {
		.mask = PALMAS_INT3_STATUS_GPADC_AUTO_1,
		.reg_offset = 2,
	},
	[PALMAS_GPADC_EOC_SW_IRQ] = {
		.mask = PALMAS_INT3_STATUS_GPADC_EOC_SW,
		.reg_offset = 2,
	},
	[PALMAS_GPADC_EOC_RT_IRQ] = {
		.mask = PALMAS_INT3_STATUS_GPADC_EOC_RT,
		.reg_offset = 2,
	},
	[PALMAS_ID_OTG_IRQ] = {
		.mask = PALMAS_INT3_STATUS_ID_OTG,
		.reg_offset = 2,
	},
	[PALMAS_ID_IRQ] = {
		.mask = PALMAS_INT3_STATUS_ID,
		.reg_offset = 2,
	},
	[PALMAS_VBUS_OTG_IRQ] = {
		.mask = PALMAS_INT3_STATUS_VBUS_OTG,
		.reg_offset = 2,
	},
	[PALMAS_VBUS_IRQ] = {
		.mask = PALMAS_INT3_STATUS_VBUS,
		.reg_offset = 2,
	},
	/* INT4 IRQs */
	[PALMAS_GPIO_0_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_0,
		.reg_offset = 3,
	},
	[PALMAS_GPIO_1_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_1,
		.reg_offset = 3,
	},
	[PALMAS_GPIO_2_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_2,
		.reg_offset = 3,
	},
	[PALMAS_GPIO_3_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_3,
		.reg_offset = 3,
	},
	[PALMAS_GPIO_4_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_4,
		.reg_offset = 3,
	},
	[PALMAS_GPIO_5_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_5,
		.reg_offset = 3,
	},
	[PALMAS_GPIO_6_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_6,
		.reg_offset = 3,
	},
	[PALMAS_GPIO_7_IRQ] = {
		.mask = PALMAS_INT4_STATUS_GPIO_7,
		.reg_offset = 3,
	},
};

static struct regmap_irq_chip palmas_irq_chip = {
	.name = "palmas",
	.irqs = palmas_irqs,
	.num_irqs = ARRAY_SIZE(palmas_irqs),

	.num_regs = 4,
	.irq_reg_stride = 5,
	.status_base = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
			PALMAS_INT1_STATUS),
	.mask_base = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
			PALMAS_INT1_MASK),
};

static void palmas_dt_to_pdata(struct device_node *node,
		struct palmas_platform_data *pdata)
{
	int ret;
	u32 prop;

	ret = of_property_read_u32(node, "ti,mux_pad1", &prop);
	if (!ret) {
		pdata->mux_from_pdata = 1;
		pdata->pad1 = prop;
	}

	ret = of_property_read_u32(node, "ti,mux_pad2", &prop);
	if (!ret) {
		pdata->mux_from_pdata = 1;
		pdata->pad2 = prop;
	}

	/* The default for this register is all masked */
	ret = of_property_read_u32(node, "ti,power_ctrl", &prop);
	if (!ret)
		pdata->power_ctrl = prop;
	else
		pdata->power_ctrl = PALMAS_POWER_CTRL_NSLEEP_MASK |
					PALMAS_POWER_CTRL_ENABLE1_MASK |
					PALMAS_POWER_CTRL_ENABLE2_MASK;
}

static int palmas_print_info(struct palmas *palmas)
{
	int ret;
	unsigned int reg, addr;
	int slave;

	/* Read varient info from the device */
	slave = PALMAS_BASE_TO_SLAVE(PALMAS_ID_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_ID_BASE, PALMAS_PRODUCT_ID_LSB);
	ret = regmap_read(palmas->regmap[slave], addr, &reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Unable to read ID err: %d\n", ret);
		goto err;
	}

	palmas->id = reg;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_ID_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_ID_BASE, PALMAS_PRODUCT_ID_MSB);
	ret = regmap_read(palmas->regmap[slave], addr, &reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Unable to read ID err: %d\n", ret);
		goto err;
	}

	palmas->id |= reg << 8;

	dev_info(palmas->dev, "Product ID %x\n", palmas->id);

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_DESIGNREV_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_DESIGNREV_BASE, PALMAS_DESIGNREV);
	ret = regmap_read(palmas->regmap[slave], addr, &reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Unable to read DESIGNREV err: %d\n", ret);
		goto err;
	}

	palmas->designrev = reg & PALMAS_DESIGNREV_DESIGNREV_MASK;

	dev_info(palmas->dev, "Product Design Rev %x\n", palmas->designrev);

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PMU_CONTROL_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE, PALMAS_SW_REVISION);
	ret = regmap_read(palmas->regmap[slave], addr, &reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Unable to read SW_REVISION err: %d\n",
			ret);
		goto err;
	}

	palmas->sw_revision = reg;

	dev_info(palmas->dev, "Product SW Rev %x\n", palmas->sw_revision);
	return 0;
err:
	return ret;
}

static struct palmas_pmic_data palmas_data = {
	.irq_chip = &palmas_irq_chip,
	.regmap_config = palmas_regmap_config,
	.mfd_cell = palmas_children,
	.id = TWL6035,
	.has_usb = 1,
};

static struct palmas_pmic_data tps659038_data = {
	.irq_chip = &palmas_irq_chip,
	.regmap_config = palmas_regmap_config,
	.mfd_cell = tps659038_children,
	.id = TPS659038,
	.has_usb = 0,
};

static const struct of_device_id of_palmas_match_tbl[] = {
	{
		.compatible = "ti,palmas",
		.data = &palmas_data,
	},
	{
		.compatible = "ti,tps659038",
		.data = &tps659038_data,
	},
	{ },
};

static int palmas_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct palmas *palmas;
	struct palmas_platform_data *pdata;
	struct device_node *node = i2c->dev.of_node;
	int ret = 0, i;
	unsigned int reg, addr;
	int slave;
	struct mfd_cell *children;
	const struct of_device_id *match;
	const struct palmas_pmic_data *pmic_data;

	pdata = dev_get_platdata(&i2c->dev);

	if (node && !pdata) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);

		if (!pdata)
			return -ENOMEM;

		palmas_dt_to_pdata(node, pdata);
	}

	if (!pdata)
		return -EINVAL;

	palmas = devm_kzalloc(&i2c->dev, sizeof(struct palmas), GFP_KERNEL);
	if (palmas == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, palmas);
	palmas->dev = &i2c->dev;
	palmas->palmas_id = id->driver_data;
	palmas->irq = i2c->irq;

	match = of_match_device(of_match_ptr(of_palmas_match_tbl), &i2c->dev);

	if (match) {
		pmic_data = match->data;
		palmas->palmas_id = pmic_data->id;
	} else {
		return -ENODATA;
	}

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
		palmas->regmap[i] = devm_regmap_init_i2c(palmas->i2c_clients[i],
				pmic_data->regmap_config + i);
		if (IS_ERR(palmas->regmap[i])) {
			ret = PTR_ERR(palmas->regmap[i]);
			dev_err(palmas->dev,
				"Failed to allocate regmap %d, err: %d\n",
				i, ret);
			goto err;
		}
	}

	/* Avoid irq requesting for TOS659038 as the IRQ line
		is only connected to a test point */
	if (palmas->palmas_id != TPS659038) {
		/* Change IRQ into clear on read mode for efficiency */
		slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);
		addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
					  PALMAS_INT_CTRL);
		reg = PALMAS_INT_CTRL_INT_CLEAR;

		regmap_write(palmas->regmap[slave], addr, reg);

		ret = regmap_add_irq_chip(palmas->regmap[slave], palmas->irq,
				IRQF_ONESHOT, 0, pmic_data->irq_chip,
				&palmas->irq_data);
		if (ret < 0)
			goto err;
	}

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PU_PD_OD_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD1);

	if (pdata->mux_from_pdata) {
		reg = pdata->pad1;
		ret = regmap_write(palmas->regmap[slave], addr, reg);
		if (ret)
			goto err_irq;
	} else {
		ret = regmap_read(palmas->regmap[slave], addr, &reg);
		if (ret)
			goto err_irq;
	}

	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_0))
		palmas->gpio_muxed |= PALMAS_GPIO_0_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_1_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_MASK) ==
			(2 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_SHIFT))
		palmas->led_muxed |= PALMAS_LED1_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_MASK) ==
			(3 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_SHIFT))
		palmas->pwm_muxed |= PALMAS_PWM1_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_2_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_MASK) ==
			(2 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_SHIFT))
		palmas->led_muxed |= PALMAS_LED2_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_MASK) ==
			(3 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_SHIFT))
		palmas->pwm_muxed |= PALMAS_PWM2_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_3))
		palmas->gpio_muxed |= PALMAS_GPIO_3_MUXED;

	addr = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD2);

	if (pdata->mux_from_pdata) {
		reg = pdata->pad2;
		ret = regmap_write(palmas->regmap[slave], addr, reg);
		if (ret)
			goto err_irq;
	} else {
		ret = regmap_read(palmas->regmap[slave], addr, &reg);
		if (ret)
			goto err_irq;
	}

	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_4))
		palmas->gpio_muxed |= PALMAS_GPIO_4_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_5_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_5_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_6))
		palmas->gpio_muxed |= PALMAS_GPIO_6_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_7_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_7_MUXED;

	dev_info(palmas->dev, "Muxing GPIO %x, PWM %x, LED %x\n",
			palmas->gpio_muxed, palmas->pwm_muxed,
			palmas->led_muxed);

	reg = pdata->power_ctrl;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PMU_CONTROL_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PMU_CONTROL_BASE, PALMAS_POWER_CTRL);

	ret = regmap_write(palmas->regmap[slave], addr, reg);
	if (ret)
		goto err_irq;

	palmas_print_info(palmas);

	/*
	 * If we are probing with DT do this the DT way and return here
	 * otherwise continue and add devices using mfd helpers.
	 */
	if (node) {
		ret = of_platform_populate(node, NULL, NULL, &i2c->dev);
		if (ret < 0)
			goto err_irq;
		else
			return ret;
	}

	/*
	 * For now all PMICs belonging to palmas family are assumed to get the
	 * same childern as PALMAS
	 */
	children = kmemdup(palmas_children, sizeof(palmas_children),
			   GFP_KERNEL);
	if (!children) {
		ret = -ENOMEM;
		goto err_irq;
	}

	children[PALMAS_PMIC_ID].platform_data = pdata->pmic_pdata;
	children[PALMAS_PMIC_ID].pdata_size = sizeof(*pdata->pmic_pdata);

	children[PALMAS_GPADC_ID].platform_data = pdata->gpadc_pdata;
	children[PALMAS_GPADC_ID].pdata_size = sizeof(*pdata->gpadc_pdata);

	children[PALMAS_RESOURCE_ID].platform_data = pdata->resource_pdata;
	children[PALMAS_RESOURCE_ID].pdata_size =
			sizeof(*pdata->resource_pdata);

	/* TPS659038 does not have USB */
	if (pmic_data->has_usb) {
		children[PALMAS_USB_ID].platform_data = pdata->usb_pdata;
		children[PALMAS_USB_ID].pdata_size = sizeof(*pdata->usb_pdata);
	}

	children[PALMAS_CLK_ID].platform_data = pdata->clk_pdata;
	children[PALMAS_CLK_ID].pdata_size = sizeof(*pdata->clk_pdata);

	ret = mfd_add_devices(palmas->dev, -1,
			      children, ARRAY_SIZE(palmas_children),
			      NULL, 0,
			      regmap_irq_get_domain(palmas->irq_data));

	kfree(children);

	if (ret < 0)
		goto err_devices;

	return ret;

err_devices:
	mfd_remove_devices(palmas->dev);
err_irq:
	regmap_del_irq_chip(palmas->irq, palmas->irq_data);
err:
	return ret;
}

static int palmas_i2c_remove(struct i2c_client *i2c)
{
	struct palmas *palmas = i2c_get_clientdata(i2c);

	mfd_remove_devices(palmas->dev);
	regmap_del_irq_chip(palmas->irq, palmas->irq_data);

	return 0;
}

static const struct i2c_device_id palmas_i2c_id[] = {
	{ "palmas", TWL6035},
	{ "twl6035", TWL6035},
	{ "twl6037", TWL6037},
	{ "tps65913", TPS65913},
	{ "tps659038", TPS659038},
	{ /* end */ }
};
MODULE_DEVICE_TABLE(i2c, palmas_i2c_id);

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
