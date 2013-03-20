/*
 * tps6130x-regulator.c
 *
 * Regulator driver for TPS6130x
 *
 * Copyright (C) 2011 Texas Instrument Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tps6130x.h>

/* Regulator id */
#define TPS6130X_VOUT			0

/* Register definitions */
#define TPS6130X_REGISTER1		0x01
#define TPS6130X_REGISTER2		0x02
#define TPS6130X_REGISTER3		0x03
#define TPS6130X_REGISTER4		0x04
#define TPS6130X_REGISTER5		0x05
#define TPS6130X_REGISTER6		0x06
#define TPS6130X_REGISTER7		0x07

/* REGISTER1 bitfields */
#define TPS6130X_ENVM			0x80
#define TPS6130X_MODE_CTRL_MASK		0x60
#define TPS6130X_SHUTDOWN_MODE		(0 << 5)
#define TPS6130X_DC_LIGHT_MODE		(1 << 5)
#define TPS6130X_DC_LIGHT_FLASH_MODE	(2 << 5)
#define TPS6130X_VOUT_MODE		(3 << 5)

/* REGISTER6 bitfields */
#define TPS6130X_OV_MASK		0x0F

/* REGISTER7 bitfields */
#define TPS6130X_REVID_MASK		0x07

/* Supported voltage values for voltage regulation mode */
#define TPS6130X_VOUT_MIN_UV		3825000
#define TPS6130X_VOUT_MAX_UV		5700000
#define TPS6130X_VOUT_STEP		125000
#define TPS6130X_VOUT_MAX_SEL		0x0F
#define TPS6130X_VOUT_DEFAULT_SEL	0x09

/**
 * struct tps_info - regulator specific details
 * @name:	name for the regulator
 * @min_uV:	minimum output voltage in uV
 * @max_uV:	maximum output voltage in uV
 * @step:	output voltage resolution in uV
 * @vsel_max:	maximum output voltage selection value
 * @vsel:	current output voltage selection value
 */
struct tps_info {
	const char *name;
	int min_uV;
	int max_uV;
	int step;
	int vsel_max;
	int vsel;
};

/**
 * struct tps_pmic - PMIC details
 * @desc:	regulator descriptor - used by regulator core
 * @client:	i2c client for this chip
 * @rdev:	regulator class device - used by regulator core
 * @regmap:	for read/write access to chip registers
 * @pdata:	board specific data for regulator
 * @info:	chip constraints on regulator behavior
 * @enabled:	flag for indicating regulator state (reset/normal mode)
 */
struct tps_pmic {
	struct regulator_desc *desc;
	struct i2c_client *client;
	struct regulator_dev *rdev;
	struct regmap *regmap;
	struct tps6130x_platform_data *pdata;
	struct tps_info *info;
	int enabled;
};

static int tps6130x_chip_enable(struct tps_pmic *tps, int on)
{
	struct i2c_client *client = tps->client;
	int ret = 0;

	/* tps61301/tps61305 supports external NRESET */
	if (tps->pdata && tps->pdata->chip_enable) {
		ret = tps->pdata->chip_enable(on);
		if (ret) {
			dev_err(&client->dev, "%s: failed to enable %d\n",
				__func__, ret);
			return ret;
		}
	}

	tps->enabled = on;

	return ret;
}

/* Operations permitted on VOUT */

static int tps6130x_vout_is_enabled(struct regulator_dev *dev)
{
	struct tps_pmic *tps = rdev_get_drvdata(dev);
	unsigned int data;
	int ret, vout = 0;

	if (!tps->enabled)
		return 0;

	ret = regmap_read(tps->regmap, TPS6130X_REGISTER1, &data);
	if (ret < 0) {
		dev_err(&tps->client->dev, "%s: failed to read register 0x%x\n",
			__func__, TPS6130X_REGISTER1);
		return ret;
	}

	/* tps61300/tps61301 have ENVM to force voltage regulation mode */
	if (data & TPS6130X_ENVM)
		vout = 1;

	/* device operating in voltage regulation mode */
	if ((data & TPS6130X_MODE_CTRL_MASK) == TPS6130X_VOUT_MODE)
		vout = 1;

	return vout;
}

static int tps6130x_vout_enable(struct regulator_dev *dev)
{
	struct tps_pmic *tps = rdev_get_drvdata(dev);
	struct tps_info *info = tps->info;
	int ret;

	ret = tps6130x_chip_enable(tps, 1);
	if (ret)
		return ret;

	/* set constant voltage source mode */
	ret = regmap_update_bits(tps->regmap, TPS6130X_REGISTER1,
				 TPS6130X_MODE_CTRL_MASK, TPS6130X_VOUT_MODE);
	if (ret < 0) {
		dev_err(&tps->client->dev, "%s: failed to update register 0x%x\n",
			__func__, TPS6130X_REGISTER1);
		goto err1;
	}

	/* output voltage must be set after voltage mode has been enabled  */
	ret = regmap_update_bits(tps->regmap, TPS6130X_REGISTER6,
				 TPS6130X_OV_MASK, info->vsel);
	if (ret < 0) {
		dev_err(&tps->client->dev, "%s: failed to update register 0x%x\n",
			__func__, TPS6130X_REGISTER6);
		goto err2;
	}

	return 0;

err2:
	regmap_update_bits(tps->regmap, TPS6130X_REGISTER1,
			   TPS6130X_MODE_CTRL_MASK, TPS6130X_SHUTDOWN_MODE);
err1:
	tps6130x_chip_enable(tps, 0);

	return ret;
}

static int tps6130x_vout_disable(struct regulator_dev *dev)
{
	struct tps_pmic *tps = rdev_get_drvdata(dev);
	int ret;

	/* may not disable voltage output if ENVM is forced */
	ret = regmap_update_bits(tps->regmap, TPS6130X_REGISTER1,
				 TPS6130X_MODE_CTRL_MASK,
				 TPS6130X_SHUTDOWN_MODE);
	if (ret < 0) {
		dev_err(&tps->client->dev, "%s: failed to update register 0x%x\n",
			__func__, TPS6130X_REGISTER1);
		return ret;
	}

	ret = tps6130x_chip_enable(tps, 0);

	return ret;
}

static int tps6130x_vout_list_voltage(struct regulator_dev *dev,
				      unsigned selector)
{
	struct tps_pmic *tps = rdev_get_drvdata(dev);
	struct tps_info *info = tps->info;

	if (selector > info->vsel_max)
		return -EINVAL;

	return info->min_uV + selector * info->step;
}

static int tps6130x_vout_get_voltage(struct regulator_dev *dev)
{
	struct tps_pmic *tps = rdev_get_drvdata(dev);
	struct tps_info *info = tps->info;

	/* output voltage cannot be read from REGISTER6 */
	return info->min_uV + info->vsel * info->step;
}

static int tps6130x_vout_set_voltage(struct regulator_dev *dev,
				     int min_uV, int max_uV, unsigned *selector)
{
	struct tps_pmic *tps = rdev_get_drvdata(dev);
	struct tps_info *info = tps->info;
	int vsel, ret;

	if ((min_uV < info->min_uV) || (min_uV > info->max_uV))
		return -EINVAL;

	if ((max_uV < info->min_uV) || (max_uV > info->max_uV))
		return -EINVAL;

	vsel = DIV_ROUND_UP(min_uV - info->min_uV, info->step);

	ret = tps6130x_vout_list_voltage(dev, vsel);
	if ((ret < 0) || (ret > max_uV))
		return -EINVAL;

	/*
	 * new target voltage must be set after
	 * voltage mode operation has been enabled
	 */
	info->vsel = vsel;
	*selector = vsel;

	return 0;
}

static struct regulator_ops tps6130x_ops = {
	.is_enabled	= tps6130x_vout_is_enabled,
	.enable		= tps6130x_vout_enable,
	.disable	= tps6130x_vout_disable,
	.get_voltage	= tps6130x_vout_get_voltage,
	.set_voltage	= tps6130x_vout_set_voltage,
	.list_voltage	= tps6130x_vout_list_voltage,
};

static struct regulator_desc tps6130x_desc = {
	.name		= "VOUT",
	.id		= TPS6130X_VOUT,
	.ops		= &tps6130x_ops,
	.n_voltages	= TPS6130X_VOUT_MAX_SEL + 1,
	.type		= REGULATOR_VOLTAGE,
	.owner		= THIS_MODULE,
};

static struct regmap_config tps6130x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int tps6130x_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct tps_info *info = (void *)id->driver_data;
	struct regulator_init_data *init_data;
	struct regulator_dev *rdev;
	struct tps_pmic *tps;
	unsigned int revid;
	int ret;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "%s: Adapter doesn't support SMBus word transactions\n",
			__func__);
		return -ENODEV;
	}

	init_data = client->dev.platform_data;
	if (!init_data) {
		dev_err(&client->dev, "%s: Platform data not found\n",
			__func__);
		return -EINVAL;
	}

	tps = devm_kzalloc(&client->dev, sizeof(*tps), GFP_KERNEL);
	if (!tps) {
		dev_err(&client->dev, "%s: Memory allocation fails\n",
			__func__);
		return -ENOMEM;
	}

	tps->regmap = devm_regmap_init_i2c(client, &tps6130x_regmap_config);
	if (IS_ERR(tps->regmap)) {
		ret = PTR_ERR(tps->regmap);
		dev_err(&client->dev, "%s: Failed to allocate register map %d\n",
			__func__, ret);
		return ret;
	}

	tps->client = client;
	tps->info = info;
	tps->desc = &tps6130x_desc;

	if (init_data->driver_data)
		tps->pdata = init_data->driver_data;

	/* default output voltage: 4.950V */
	tps->info->vsel = TPS6130X_VOUT_DEFAULT_SEL;

	ret = tps6130x_chip_enable(tps, 1);
	if (ret)
		goto enable_err;

	ret = regmap_read(tps->regmap, TPS6130X_REGISTER7, &revid);
	if (ret < 0) {
		dev_err(&client->dev, "%s: Failed to access device\n",
			__func__);
		goto rev_err;
	}

	dev_info(&client->dev, "%s: Revision %d\n", __func__,
		 revid & TPS6130X_REVID_MASK);

	ret = tps6130x_chip_enable(tps, 0);
	if (ret)
		goto enable_err;

	/* register the regulator */
	rdev = regulator_register(tps->desc, &client->dev, init_data,
			tps, NULL);
	if (IS_ERR(rdev)) {
		dev_err(&client->dev, "%s: Failed to register %s\n",
			__func__, id->name);
		ret = PTR_ERR(rdev);
		goto enable_err;
	}

	tps->rdev = rdev;
	i2c_set_clientdata(client, tps);

	return 0;

rev_err:
	tps6130x_chip_enable(tps, 0);
enable_err:
	tps->client = NULL;

	return ret;
}

static int __devexit tps6130x_remove(struct i2c_client *client)
{
	struct tps_pmic *tps = i2c_get_clientdata(client);

	regulator_unregister(tps->rdev);
	tps->client = NULL;
	i2c_set_clientdata(client, NULL);

	return 0;
}

static struct tps_info tps6130x_regs = {
	.name	= "VOUT",
	.min_uV	= TPS6130X_VOUT_MIN_UV,
	.max_uV	= TPS6130X_VOUT_MAX_UV,
	.step	= TPS6130X_VOUT_STEP,
	.vsel_max = TPS6130X_VOUT_MAX_SEL,
};

static const struct i2c_device_id tps6130x_id[] = {
	{
		.name = "tps6130x",
		.driver_data = (unsigned long)&tps6130x_regs,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, tps6130x_id);

static struct i2c_driver tps6130x_i2c_driver = {
	.driver = {
		.name	= "tps6130x",
		.owner	= THIS_MODULE,
	},
	.probe		= tps6130x_probe,
	.remove		= __devexit_p(tps6130x_remove),
	.id_table	= tps6130x_id,
};

static int __init tps6130x_init(void)
{
	return i2c_add_driver(&tps6130x_i2c_driver);
}
subsys_initcall(tps6130x_init);

static void __exit tps6130x_exit(void)
{
	i2c_del_driver(&tps6130x_i2c_driver);
}
module_exit(tps6130x_exit);

MODULE_AUTHOR("Misael Lopez Cruz <misael.lopez@ti.com>");
MODULE_DESCRIPTION("TPS6130X voltage regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tps6130x-regulator");
