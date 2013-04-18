/*
 * Texas Instrument SmartReflex AVS Class 0 driver
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
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
#define pr_fmt(fmt)  KBUILD_MODNAME ": %s(): " fmt, __func__

#include <linux/err.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/string.h>

/**
 * struct tiavs_class0_data - class data for the regulator instance
 * @reg:		regulator that will actually set the voltage
 * @volt_set_table:	voltage to set data table
 * @current_idx:	current index
 * @voltage_tolerance:	% tolerance for voltage(optional)
 */
struct tiavs_class0_data {
	struct regulator *reg;
	unsigned int *volt_set_table;
	int current_idx;
	u32 voltage_tolerance;
};

/**
 * tiavs_class0_set_voltage_sel() - set voltage
 * @rdev:	regulator device
 * @sel:	set voltage corresponding to selector
 *
 * This searches for a best case match and uses the child regulator to set
 * appropriate voltage
 *
 * Return: -ENODEV if no proper regulator data/-EINVAL if no match,bad efuse
 * else returns regulator set result
 */
static int tiavs_class0_set_voltage_sel(struct regulator_dev *rdev,
					unsigned sel)
{
	struct tiavs_class0_data *data = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	struct regulator *reg;
	int vset, ret, tol;

	if (!data) {
		pr_err("No regulator drvdata\n");
		return -ENODEV;
	}

	reg = data->reg;
	if (!reg) {
		pr_err("No regulator\n");
		return -ENODEV;
	}

	if (!desc->n_voltages || !data->volt_set_table) {
		pr_err("No valid voltage table entries?\n");
		return -EINVAL;
	}

	if (sel >= desc->n_voltages) {
		pr_err("sel(%d) > max voltage table entries(%d)\n", sel,
		       desc->n_voltages);
		return -EINVAL;
	}

	vset = data->volt_set_table[sel];

	/* Adjust for % tolerance needed */
	tol = DIV_ROUND_UP(vset * data->voltage_tolerance, 100);
	ret = regulator_set_voltage_tol(reg, vset, tol);
	if (!ret)
		data->current_idx = sel;

	return ret;
}

/**
 * tiavs_class0_get_voltage_sel() - Get voltage selector
 * @rdev:	regulator device
 *
 * Return: -ENODEV if no proper regulator data/-EINVAL if no data,
 * else returns current index.
 */
static int tiavs_class0_get_voltage_sel(struct regulator_dev *rdev)
{
	const struct regulator_desc *desc = rdev->desc;
	struct tiavs_class0_data *data = rdev_get_drvdata(rdev);

	if (!data) {
		pr_err("No regulator drvdata\n");
		return -ENODEV;
	}

	if (!desc->n_voltages || !data->volt_set_table) {
		pr_err("No valid voltage table entries?\n");
		return -EINVAL;
	}

	if (data->current_idx > desc->n_voltages) {
		pr_err("Corrupted data structure?? idx(%d) > n_voltages(%d)\n",
		       data->current_idx, desc->n_voltages);
		return -EINVAL;
	}

	return data->current_idx;
}

static struct regulator_ops tiavs_class0_ops = {
	.list_voltage = regulator_list_voltage_table,

	.set_voltage_sel = tiavs_class0_set_voltage_sel,
	.get_voltage_sel = tiavs_class0_get_voltage_sel,

};

static struct regulator_desc tiavs_class0_desc = {
	.ops = &tiavs_class0_ops,
	.name = "avsclass0",
	.owner = THIS_MODULE,

};

static const struct of_device_id tiavs_class0_of_match[] = {
	{.compatible = "ti,avsclass0", .data = &tiavs_class0_desc},
	{},
};

MODULE_DEVICE_TABLE(of, tiavs_class0_of_match);

/**
 * tiavs_class0_probe() - AVS class 0 probe
 * @pdev: matching platform device
 *
 * We support only device tree provided data here. Once we find a regulator,
 * efuse offsets, we pick up the efuse register voltages store them per
 * instance.
 *
 * Return: if everything goes through, we return 0, else corresponding error
 * value is returned.
 */
static int tiavs_class0_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device_node *np = pdev->dev.of_node;
	struct property *prop;
	struct resource *res;
	struct regulator *reg;
	struct regulator_init_data *initdata = NULL;
	struct regulator_config config = { };
	struct regulation_constraints *c;
	struct regulator_dev *rdev;
	struct regulator_desc *desc;
	struct tiavs_class0_data *data;
	void __iomem *base;
	const __be32 *val;
	const void *temp;
	unsigned int *volt_table;
	bool efuse_is_uV = false;
	int proplen, i, ret;
	int reg_v, min_uV = INT_MAX, max_uV = 0;
	int best_val = INT_MAX, choice = -EINVAL;

	match = of_match_device(tiavs_class0_of_match, &pdev->dev);
	if (match) {
		temp = match->data;
		initdata = of_get_regulator_init_data(&pdev->dev, np);
	}
	if (!initdata) {
		dev_err(&pdev->dev, "No proper OF?\n");
		return -ENODEV;
	}
	if (!temp) {
		dev_err(&pdev->dev, "No proper desc?\n");
		return -ENODEV;
	}

	/* look for avs-supply */
	reg = devm_regulator_get(&pdev->dev, "avs");
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		reg = NULL;
		dev_err(&pdev->dev, "avs_class0 regulator not available(%d)\n",
			ret);
		return ret;
	}

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		dev_err(&pdev->dev, "No memory to alloc desc!\n");
		return -ENOMEM;
	}
	memcpy(desc, temp, sizeof(*desc));

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "No memory to alloc data!\n");
		return -ENOMEM;
	}
	data->reg = reg;

	/* pick up optional properties */
	of_property_read_u32(np, "voltage-tolerance", &data->voltage_tolerance);
	efuse_is_uV = of_property_read_bool(np,
					    "ti,avsclass0-microvolt-values");

	/* pick up Efuse based voltages */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get IO resource\n");
		return -ENODEV;
	}

	base = devm_request_and_ioremap(&pdev->dev, res);
	if (!base) {
		dev_err(&pdev->dev, "Unable to map Efuse registers\n");
		return -ENOMEM;
	}

	/* Fetch efuse-settings. */
	prop = of_find_property(np, "efuse-settings", NULL);
	if (!prop) {
		dev_err(&pdev->dev, "No 'efuse-settings' property found\n");
		return -EINVAL;
	}

	proplen = prop->length / sizeof(int);

	data->volt_set_table =
	    devm_kzalloc(&pdev->dev, sizeof(unsigned int) * (proplen / 2),
			 GFP_KERNEL);
	if (!data->volt_set_table) {
		dev_err(&pdev->dev, "Unable to Allocate voltage set table\n");
		return -ENOMEM;
	}

	volt_table =
	    devm_kzalloc(&pdev->dev, sizeof(unsigned int) * (proplen / 2),
			 GFP_KERNEL);
	if (!volt_table) {
		dev_err(&pdev->dev,
			"Unable to Allocate voltage lookup table\n");
		return -ENOMEM;
	}

	val = prop->value;
	for (i = 0; i < proplen / 2; i++) {
		u32 efuse_offset;

		volt_table[i] = be32_to_cpup(val++);
		efuse_offset = be32_to_cpup(val++);

		data->volt_set_table[i] = efuse_is_uV ?
		    readl(base + efuse_offset) :
		    readw(base + efuse_offset) * 1000;

		/* Find min/max for the voltage sets */
		if (min_uV > volt_table[i])
			min_uV = volt_table[i];
		if (max_uV < volt_table[i])
			max_uV = volt_table[i];

		dev_dbg(&pdev->dev, "[%d] efuse=0x%08x volt_table=%d vset=%d\n",
			i, efuse_offset, volt_table[i],
			data->volt_set_table[i]);
	}
	desc->n_voltages = i;
	desc->volt_table = volt_table;

	/* Search for a best match voltage */
	reg_v = regulator_get_voltage(reg);
	if (reg_v < 0) {
		dev_err(&pdev->dev, "Regulator error %d for get_voltage!\n",
			reg_v);
		return reg_v;
	}

	for (i = 0; i < desc->n_voltages; i++)
		if (data->volt_set_table[i] < best_val &&
		    data->volt_set_table[i] >= reg_v) {
			best_val = data->volt_set_table[i];
			choice = i;
		}

	if (choice == -EINVAL) {
		dev_err(&pdev->dev, "No match regulator V=%d\n", reg_v);
		return -EINVAL;
	}
	data->current_idx = choice;

	/*
	 * Constrain board-specific capabilities according to what
	 * this driver can actually do.
	 */
	c = &initdata->constraints;
	if (desc->n_voltages > 1)
		c->valid_ops_mask |= REGULATOR_CHANGE_VOLTAGE;
	c->always_on = true;

	c->min_uV = min_uV;
	c->max_uV = max_uV;

	config.dev = &pdev->dev;
	config.init_data = initdata;
	config.driver_data = data;
	config.of_node = pdev->dev.of_node;

	rdev = regulator_register(desc, &config);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "can't register %s, %ld\n",
			desc->name, PTR_ERR(rdev));
		return PTR_ERR(rdev);
	}
	platform_set_drvdata(pdev, rdev);

	return 0;
}

static int tiavs_class0_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);
	return 0;
}

MODULE_ALIAS("platform:tiavs_class0");

static struct platform_driver tiavs_class0_driver = {
	.probe = tiavs_class0_probe,
	.remove = tiavs_class0_remove,
	.driver = {
		   .name = "tiavs_class0",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(tiavs_class0_of_match),
		   },
};
module_platform_driver(tiavs_class0_driver);

MODULE_DESCRIPTION("TI SmartReflex AVS class 0 regulator driver");
MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_LICENSE("GPL v2");
