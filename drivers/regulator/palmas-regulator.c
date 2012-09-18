/*
 * Driver for Regulator part of Palmas PMIC Chips
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/mfd/palmas.h>

struct regs_info {
	char	*name;
	u8	vsel_addr;
	u8	ctrl_addr;
	u8	tstep_addr;
};

static struct regs_info palmas_regs_info[] = {
	{
		.name		= "SMPS12",
		.vsel_addr	= PALMAS_SMPS12_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS12_CTRL,
		.tstep_addr	= PALMAS_SMPS12_TSTEP,
	},
	{
		.name		= "SMPS123",
		.vsel_addr	= PALMAS_SMPS12_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS12_CTRL,
		.tstep_addr	= PALMAS_SMPS12_TSTEP,
	},
	{
		.name		= "SMPS3",
		.vsel_addr	= PALMAS_SMPS3_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS3_CTRL,
	},
	{
		.name		= "SMPS45",
		.vsel_addr	= PALMAS_SMPS45_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS45_CTRL,
		.tstep_addr	= PALMAS_SMPS45_TSTEP,
	},
	{
		.name		= "SMPS457",
		.vsel_addr	= PALMAS_SMPS45_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS45_CTRL,
		.tstep_addr	= PALMAS_SMPS45_TSTEP,
	},
	{
		.name		= "SMPS6",
		.vsel_addr	= PALMAS_SMPS6_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS6_CTRL,
		.tstep_addr	= PALMAS_SMPS6_TSTEP,
	},
	{
		.name		= "SMPS7",
		.vsel_addr	= PALMAS_SMPS7_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS7_CTRL,
	},
	{
		.name		= "SMPS8",
		.vsel_addr	= PALMAS_SMPS8_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS8_CTRL,
		.tstep_addr	= PALMAS_SMPS8_TSTEP,
	},
	{
		.name		= "SMPS9",
		.vsel_addr	= PALMAS_SMPS9_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS9_CTRL,
	},
	{
		.name		= "SMPS10",
	},
	{
		.name		= "LDO1",
		.vsel_addr	= PALMAS_LDO1_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO1_CTRL,
	},
	{
		.name		= "LDO2",
		.vsel_addr	= PALMAS_LDO2_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO2_CTRL,
	},
	{
		.name		= "LDO3",
		.vsel_addr	= PALMAS_LDO3_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO3_CTRL,
	},
	{
		.name		= "LDO4",
		.vsel_addr	= PALMAS_LDO4_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO4_CTRL,
	},
	{
		.name		= "LDO5",
		.vsel_addr	= PALMAS_LDO5_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO5_CTRL,
	},
	{
		.name		= "LDO6",
		.vsel_addr	= PALMAS_LDO6_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO6_CTRL,
	},
	{
		.name		= "LDO7",
		.vsel_addr	= PALMAS_LDO7_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO7_CTRL,
	},
	{
		.name		= "LDO8",
		.vsel_addr	= PALMAS_LDO8_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO8_CTRL,
	},
	{
		.name		= "LDO9",
		.vsel_addr	= PALMAS_LDO9_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO9_CTRL,
	},
	{
		.name		= "LDOLN",
		.vsel_addr	= PALMAS_LDOLN_VOLTAGE,
		.ctrl_addr	= PALMAS_LDOLN_CTRL,
	},
	{
		.name		= "LDOUSB",
		.vsel_addr	= PALMAS_LDOUSB_VOLTAGE,
		.ctrl_addr	= PALMAS_LDOUSB_CTRL,
	},
};

#define SMPS_CTRL_MODE_OFF		0x00
#define SMPS_CTRL_MODE_ON		0x01
#define SMPS_CTRL_MODE_ECO		0x02
#define SMPS_CTRL_MODE_PWM		0x03

/* These values are derived from the data sheet. And are the number of steps
 * where there is a voltage change, the ranges at beginning and end of register
 * max/min values where there are no change are ommitted.
 *
 * So they are basically (maxV-minV)/stepV
 */
#define PALMAS_SMPS_NUM_VOLTAGES	116
#define PALMAS_SMPS10_NUM_VOLTAGES	2
#define PALMAS_LDO_NUM_VOLTAGES		50

#define SMPS10_VSEL			(1<<3)
#define SMPS10_BOOST_EN			(1<<2)
#define SMPS10_BYPASS_EN		(1<<1)
#define SMPS10_SWITCH_EN		(1<<0)

/* SMPS PHASE REG VALUES */
#define SMPS_PHASE_SELECTION_AUTOMATIC		0x00
#define SMPS_PHASE_SELECTION_FORCE_SINGLE	0x01
#define SMPS_PHASE_SELECTION_FORCE_MULTI	0x02

static int palmas_smps_read(struct palmas *palmas, unsigned int reg,
		unsigned int *dest)
{
	int slave;
	unsigned int addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_SMPS_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_SMPS_BASE, reg);

	return regmap_read(palmas->regmap[slave], addr, dest);
}

static int palmas_smps_write(struct palmas *palmas, unsigned int reg,
		unsigned int value)
{
	int slave;
	unsigned int addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_SMPS_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_SMPS_BASE, reg);

	return regmap_write(palmas->regmap[slave], addr, value);
}

static int palmas_ldo_read(struct palmas *palmas, unsigned int reg,
		unsigned int *dest)
{
	int slave;
	unsigned int addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_LDO_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_LDO_BASE, reg);

	return regmap_read(palmas->regmap[slave], addr, dest);
}

static int palmas_ldo_write(struct palmas *palmas, unsigned int reg,
		unsigned int value)
{
	int slave;
	unsigned int addr;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_LDO_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_LDO_BASE, reg);

	return regmap_write(palmas->regmap[slave], addr, value);
}

static int palmas_is_enabled_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= SMPS12_CTRL_STATUS_MASK;
	reg >>= SMPS12_CTRL_STATUS_SHIFT;

	return !!(reg);
}

static int palmas_enable_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= ~SMPS12_CTRL_MODE_ACTIVE_MASK;
	reg |= SMPS_CTRL_MODE_ON;

	palmas_smps_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static int palmas_disable_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= ~SMPS12_CTRL_MODE_ACTIVE_MASK;

	palmas_smps_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}


static int palmas_set_mode_smps(struct regulator_dev *dev, unsigned int mode)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);
	reg &= ~SMPS12_CTRL_STATUS_MASK;
	reg >>= SMPS12_CTRL_STATUS_SHIFT;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		reg |= SMPS_CTRL_MODE_ON;
		break;
	case REGULATOR_MODE_IDLE:
		reg |= SMPS_CTRL_MODE_ECO;
		break;
	case REGULATOR_MODE_FAST:
		reg |= SMPS_CTRL_MODE_PWM;
		break;
	default:
		return -EINVAL;
	}
	palmas_smps_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static unsigned int palmas_get_mode_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);
	reg &= SMPS12_CTRL_STATUS_MASK;
	reg >>= SMPS12_CTRL_STATUS_SHIFT;

	switch (reg) {
	case SMPS_CTRL_MODE_ON:
		return REGULATOR_MODE_NORMAL;
	case SMPS_CTRL_MODE_ECO:
		return REGULATOR_MODE_IDLE;
	case SMPS_CTRL_MODE_PWM:
		return REGULATOR_MODE_FAST;
	}

	return 0;
}

static int palmas_list_voltage_smps(struct regulator_dev *dev,
					unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	int mult = 1;

	if (!selector)
		return 0;

	/* Read the multiplier set in VSEL register to return
	 * the correct voltage.
	 */
	if (pmic->range[id])
		mult = 2;

	/* Voltage is (0.49V + (selector * 0.01V)) * RANGE
	 * as defined in data sheet. RANGE is either x1 or x2
	 */
	return  (490000 + (selector * 10000)) * mult;
}

static int palmas_get_voltage_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	int selector;
	unsigned int reg;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	palmas_smps_read(pmic->palmas, addr, &reg);

	selector = reg & SMPS12_VOLTAGE_VSEL_MASK;

	/* Adjust selector to match list_voltage ranges */
	if ((selector > 0) && (selector < 6))
		selector = 6;
	if (!selector)
		selector = 5;
	if (selector > 121)
		selector = 121;
	selector -= 5;

	return palmas_list_voltage_smps(dev, selector);
}

static int palmas_set_voltage_smps_sel(struct regulator_dev *dev,
		unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg = 0;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	/* Make sure we don't change the value of RANGE */
	if (pmic->range[id])
		reg |= SMPS12_VOLTAGE_RANGE;

	/* Adjust the linux selector into range used in VSEL register */
	if (selector)
		reg |= selector + 5;

	palmas_smps_write(pmic->palmas, addr, reg);

	return 0;
}

static struct regulator_ops palmas_ops_smps = {
	.is_enabled		= palmas_is_enabled_smps,
	.enable			= palmas_enable_smps,
	.disable		= palmas_disable_smps,
	.set_mode		= palmas_set_mode_smps,
	.get_mode		= palmas_get_mode_smps,
	.get_voltage		= palmas_get_voltage_smps,
	.set_voltage_sel	= palmas_set_voltage_smps_sel,
	.list_voltage		= palmas_list_voltage_smps,
};

static int palmas_is_enabled_smps10(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, PALMAS_SMPS10_STATUS, &reg);

	reg &= SMPS10_BOOST_EN;

	return !!(reg);
}

static int palmas_enable_smps10(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, PALMAS_SMPS10_CTRL, &reg);

	reg &= ~SMPS10_BOOST_EN;
	reg |= SMPS10_BOOST_EN;

	palmas_smps_write(pmic->palmas, PALMAS_SMPS10_CTRL, reg);

	return 0;
}

static int palmas_disable_smps10(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, PALMAS_SMPS10_CTRL, &reg);

	reg &= ~SMPS10_BOOST_EN;

	palmas_smps_write(pmic->palmas, PALMAS_SMPS10_CTRL, reg);

	return 0;
}

static int palmas_list_voltage_smps10(struct regulator_dev *dev,
					unsigned selector)
{
	if (selector)
		return 5000000;

	return 3750000;
}

static int palmas_get_voltage_smps10(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int selector;
	unsigned int reg;

	palmas_smps_read(pmic->palmas, PALMAS_SMPS10_CTRL, &reg);

	selector = (reg & SMPS10_VSEL) >> 3;

	return palmas_list_voltage_smps10(dev, selector);
}

static int palmas_set_voltage_smps10_sel(struct regulator_dev *dev,
		unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	unsigned int reg = 0;

	if (selector)
		reg |= SMPS10_VSEL;
	else
		reg &= SMPS10_VSEL;

	palmas_smps_write(pmic->palmas, PALMAS_SMPS10_CTRL, reg);

	return 0;
}

/* @brief set or clear the bypass bit on SMPS10
 *
 * There is not a way to represent this function within the regulator
 * framework. This sets/clears the bypass of SMPS10 so voltage is obtained
 * from either SMPS10_IN or BOOST.
 *
 * @param palmas pointer to the palmas mfd structure
 * @param bypass boolean to indicate switch status
 * @return error or result
 */
int palmas_set_bypass_smps10(struct palmas *palmas, int bypass)
{
	unsigned int reg;

	palmas_smps_read(palmas, PALMAS_SMPS10_CTRL, &reg);

	if (bypass)
		reg |= SMPS10_BYPASS_EN;
	else
		reg &= ~SMPS10_BYPASS_EN;

	palmas_smps_write(palmas, PALMAS_SMPS10_CTRL, reg);

	return 0;
}
EXPORT_SYMBOL(palmas_set_bypass_smps10);

/* @brief set or clear the switch bit on SMPS10
 *
 * There is not a way to represent this function within the regulator
 * framework. This sets/clears the switch of SMPS10 so SMPS10_OUT1 and
 * SMPS10_OUT2 are shorted together.
 *
 * @param palmas pointer to the palmas mfd structure
 * @param sw boolean to indicate switch status
 * @return error or result
 */
int palmas_set_switch_smps10(struct palmas *palmas, int sw)
{
	unsigned int reg;

	palmas_smps_read(palmas, PALMAS_SMPS10_CTRL, &reg);

	if (sw)
		reg |= SMPS10_SWITCH_EN;
	else
		reg &= ~SMPS10_SWITCH_EN;

	palmas_smps_write(palmas, PALMAS_SMPS10_CTRL, reg);

	return 0;
}
EXPORT_SYMBOL(palmas_set_switch_smps10);

static struct regulator_ops palmas_ops_smps10 = {
	.is_enabled		= palmas_is_enabled_smps10,
	.enable			= palmas_enable_smps10,
	.disable		= palmas_disable_smps10,
	.get_voltage		= palmas_get_voltage_smps10,
	.set_voltage_sel	= palmas_set_voltage_smps10_sel,
	.list_voltage		= palmas_list_voltage_smps10,
};

static int palmas_is_enabled_ldo(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_ldo_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= LDO1_CTRL_STATUS;

	return !!(reg);
}

static int palmas_enable_ldo(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_ldo_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg |= LDO1_CTRL_MODE_ACTIVE;

	palmas_ldo_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static int palmas_disable_ldo(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_ldo_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= ~LDO1_CTRL_MODE_ACTIVE;

	palmas_ldo_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static int palmas_list_voltage_ldo(struct regulator_dev *dev,
					unsigned selector)
{
	if (!selector)
		return 0;

	/* voltage is 0.85V + (selector * 0.05v) */
	return  850000 + (selector * 50000);
}

static int palmas_get_voltage_ldo(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	int selector;
	unsigned int reg;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	palmas_ldo_read(pmic->palmas, addr, &reg);

	selector = reg & LDO1_VOLTAGE_VSEL_MASK;

	/* Adjust selector to match list_voltage ranges */
	if (selector > 49)
		selector = 49;

	return palmas_list_voltage_ldo(dev, selector);
}

static int palmas_set_voltage_ldo_sel(struct regulator_dev *dev,
		unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg = 0;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	reg = selector;

	palmas_ldo_write(pmic->palmas, addr, reg);

	return 0;
}

/* @brief set or clear the tracking bit on LDO8
 *
 * This sets or clears the tracking enabled on LDO8
 *
 * @param palmas pointer to the palmas mfd structure
 * @param tracking a boolean to set/clear tracking
 * @return error or result
 */
int palmas_set_ldo8_tracking(struct palmas *palmas, int tracking)
{
	int ret;
	unsigned int reg;

	ret = palmas_smps_read(palmas, PALMAS_LDO8_CTRL, &reg);
	if (ret)
		return ret;

	if (tracking)
		reg |= LDO8_CTRL_LDO_TRACKING_EN;
	else
		reg &= ~LDO8_CTRL_LDO_TRACKING_EN;

	ret = palmas_smps_write(palmas, PALMAS_LDO8_CTRL, reg);

	return ret;
}
EXPORT_SYMBOL(palmas_set_ldo8_tracking);

/* @brief set or clear the bypass bit on LDO9
 *
 * This sets or clears the bypass enabled on LDO9
 *
 * @param palmas pointer to the palmas mfd structure
 * @param bypass a boolean to set/clear bypass
 * @return error or success
 */
int palmas_set_ldo9_bypass(struct palmas *palmas, int bypass)
{
	int ret;
	unsigned int reg;

	ret = palmas_ldo_read(palmas, PALMAS_LDO9_CTRL, &reg);
	if (ret)
		return ret;

	if (bypass)
		reg |= LDO9_CTRL_LDO_BYPASS_EN;
	else
		reg &= ~LDO9_CTRL_LDO_BYPASS_EN;

	ret = palmas_ldo_write(palmas, PALMAS_LDO9_CTRL, reg);

	return ret;
}
EXPORT_SYMBOL(palmas_set_ldo9_bypass);

static int palmas_smps_init(struct palmas *palmas, int id,
		struct palmas_reg_init *reg_init)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[id].ctrl_addr;

	ret = palmas_smps_read(palmas, addr, &reg);
	if (ret)
		return ret;

	if (id != PALMAS_REG_SMPS10) {
		/* if we have the erratum, OVERRIDE to FORCE_PWM */
		if (is_palmas_erratum(palmas, SMPS_OUTPUT_VOLT_DROP)) {
			reg_init->mode_sleep = SMPS_CTRL_MODE_PWM;
			reg_init->mode_active = SMPS_CTRL_MODE_PWM;
		}

		if (reg_init->warm_reset)
			reg |= SMPS12_CTRL_WR_S;

		if (reg_init->roof_floor)
			reg |= SMPS12_CTRL_ROOF_FLOOR_EN;

		if (reg_init->mode_sleep) {
			reg &= ~SMPS12_CTRL_MODE_SLEEP_MASK;
			reg |= reg_init->mode_sleep <<
					SMPS12_CTRL_MODE_SLEEP_SHIFT;
		}
		if (reg_init->mode_active) {
			reg &= ~SMPS12_CTRL_MODE_ACTIVE_MASK;
			reg |= reg_init->mode_active <<
					SMPS12_CTRL_MODE_ACTIVE_SHIFT;
		}
	} else {
		if (reg_init->mode_sleep) {
			reg &= ~SMPS10_CTRL_MODE_SLEEP_MASK;
			reg |= reg_init->mode_sleep <<
					SMPS10_CTRL_MODE_SLEEP_SHIFT;
		}
		if (reg_init->mode_active) {
			reg &= ~SMPS10_CTRL_MODE_ACTIVE_MASK;
			reg |= reg_init->mode_active <<
					SMPS10_CTRL_MODE_ACTIVE_SHIFT;
		}
	}
	ret = palmas_smps_write(palmas, addr, reg);
	if (ret)
		return ret;

	if (palmas_regs_info[id].tstep_addr && reg_init->tstep) {
		addr = palmas_regs_info[id].tstep_addr;

		reg = reg_init->tstep & SMPS12_TSTEP_TSTEP_MASK;

		ret = palmas_smps_write(palmas, addr, reg);
		if (ret)
			return ret;
	}

	if (palmas_regs_info[id].vsel_addr && reg_init->vsel) {
		addr = palmas_regs_info[id].vsel_addr;

		reg = reg_init->vsel;

		ret = palmas_smps_write(palmas, addr, reg);
		if (ret)
			return ret;
	}


	return 0;
}

static int palmas_ldo_init(struct palmas *palmas, int id,
		struct palmas_reg_init *reg_init)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[id].ctrl_addr;

	ret = palmas_smps_read(palmas, addr, &reg);
	if (ret)
		return ret;

	if (reg_init->warm_reset)
		reg |= LDO1_CTRL_WR_S;

	if (reg_init->mode_sleep)
		reg |= LDO1_CTRL_MODE_SLEEP;

	ret = palmas_smps_write(palmas, addr, reg);
	if (ret)
		return ret;

	if (id == PALMAS_REG_LDO9) {
		if (reg_init->no_bypass)
			palmas_set_ldo9_bypass(palmas, 0);
		else
			palmas_set_ldo9_bypass(palmas, 1);
	}

	return 0;
}

static struct regulator_ops palmas_ops_ldo = {
	.is_enabled		= palmas_is_enabled_ldo,
	.enable			= palmas_enable_ldo,
	.disable		= palmas_disable_ldo,
	.get_voltage		= palmas_get_voltage_ldo,
	.set_voltage_sel	= palmas_set_voltage_ldo_sel,
	.list_voltage		= palmas_list_voltage_ldo,
};

static __devinit int palmas_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_platform_data *pdata = palmas->dev->platform_data;
	struct regulator_init_data *reg_data;
	struct regulator_dev *rdev;
	struct palmas_pmic *pmic;
	struct palmas_reg_init *reg_init;
	int id = 0, ret;
	unsigned int addr, reg;

	if (!pdata)
		return -EINVAL;
	if (!pdata->pmic_pdata)
		return -EINVAL;
	if (!pdata->pmic_pdata->reg_data)
		return -EINVAL;

	pmic = kzalloc(sizeof(*pmic), GFP_KERNEL);
	if (!pmic)
		return -ENOMEM;

	pmic->dev = &pdev->dev;
	pmic->palmas = palmas;
	palmas->pmic = pmic;
	platform_set_drvdata(pdev, pmic);

	pmic->desc = kcalloc(PALMAS_NUM_REGS,
			sizeof(struct regulator_desc), GFP_KERNEL);
	if (!pmic->desc) {
		ret = -ENOMEM;
		goto err_free_pmic;
	}

	pmic->rdev = kcalloc(PALMAS_NUM_REGS,
			sizeof(struct regulator_dev *), GFP_KERNEL);
	if (!pmic->rdev) {
		ret = -ENOMEM;
		goto err_free_desc;
	}

	ret = palmas_smps_read(palmas, PALMAS_SMPS_CTRL, &reg);
	if (ret)
		goto err_unregister_regulator;

	if (reg & SMPS_CTRL_SMPS12_SMPS123_EN)
		pmic->smps123 = 1;

	if (reg & SMPS_CTRL_SMPS45_SMPS457_EN)
		pmic->smps457 = 1;

	/*
	 * Errata Registration
	 * TBD - Handle based on revision once we are sure of fix
	 */

	if (palmas->id == PALMAS_ID_TWL6035 &&
	    palmas->revision <= PALMAS_REV_ES2_0) {
		set_palmas_erratum(palmas, SMPS_OUTPUT_VOLT_DROP);
		dev_warn(&pdev->dev, "Erratum SMPS_OUTPUT_VOLT_DROP!");
	}

	if (palmas->id == PALMAS_ID_TWL6035 &&
	    palmas->revision <= PALMAS_REV_ES2_0) {
		set_palmas_erratum(palmas, SMPS_MIXED_PHASE_ZERO_CROSS_DETECT);
		dev_warn(&pdev->dev,
			 "Erratum SMPS_MIXED_PHASE_ZERO_CROSS_DETECT!");
	}

	if (is_palmas_erratum(palmas, SMPS_MIXED_PHASE_ZERO_CROSS_DETECT)) {
		ret = palmas_smps_read(palmas, PALMAS_SMPS_CTRL, &reg);
		if (ret)
			goto err_unregister_regulator;

		/*
		 * Dont bother to check if enabled, set the config and usage
		 * determine the result.
		 */
		reg &= ~(SMPS_CTRL_SMPS45_PHASE_CTRL_MASK |
			 SMPS_CTRL_SMPS123_PHASE_CTRL_MASK);
		reg |= SMPS_PHASE_SELECTION_FORCE_MULTI <<
			SMPS_CTRL_SMPS45_PHASE_CTRL_SHIFT;
		reg |= SMPS_PHASE_SELECTION_FORCE_MULTI <<
			SMPS_CTRL_SMPS123_PHASE_CTRL_SHIFT;

		ret = palmas_smps_write(palmas, PALMAS_SMPS_CTRL, reg);
		if (ret)
			goto err_unregister_regulator;
	}

	for (id = 0; id < PALMAS_REG_LDO1; id++) {

		/*
		 * Miss out regulators which are not available due
		 * to slaving configurations.
		 */
		switch (id) {
		case PALMAS_REG_SMPS12:
		case PALMAS_REG_SMPS3:
			if (pmic->smps123)
				continue;
			break;
		case PALMAS_REG_SMPS123:
			if (!pmic->smps123)
				continue;
			break;
		case PALMAS_REG_SMPS45:
		case PALMAS_REG_SMPS7:
			if (pmic->smps457)
				continue;
			break;
		case PALMAS_REG_SMPS457:
			if (!pmic->smps457)
				continue;
		}

		reg_data = pdata->pmic_pdata->reg_data[id];
		if (!reg_data)
			continue;

		/* Register the regulators */
		pmic->desc[id].name = palmas_regs_info[id].name;
		pmic->desc[id].id = id;

		if (id != PALMAS_REG_SMPS10) {
			pmic->desc[id].ops = &palmas_ops_smps;
			pmic->desc[id].n_voltages = PALMAS_SMPS_NUM_VOLTAGES;
		} else {
			pmic->desc[id].n_voltages = PALMAS_SMPS10_NUM_VOLTAGES;
			pmic->desc[id].ops = &palmas_ops_smps10;
		}

		pmic->desc[id].type = REGULATOR_VOLTAGE;
		pmic->desc[id].owner = THIS_MODULE;

		/* Initialise sleep/init values from platform data */
		if (pdata->pmic_pdata->reg_init) {
			reg_init = pdata->pmic_pdata->reg_init[id];
			if (reg_init) {
				ret = palmas_smps_init(palmas, id, reg_init);
				if (ret)
					goto err_unregister_regulator;
			}
		}

		/*
		 * read and store the RANGE bit for later use
		 * This must be done before regulator is probed otherwise
		 * we error in probe with unsuportable ranges.
		 */
		if (id != PALMAS_REG_SMPS10) {
			addr = palmas_regs_info[id].vsel_addr;

			ret = palmas_smps_read(pmic->palmas, addr, &reg);
			if (ret)
				goto err_unregister_regulator;
			if (reg & SMPS12_VOLTAGE_RANGE)
				pmic->range[id] = 1;
		}

		rdev = regulator_register(&pmic->desc[id],
				&pdev->dev, reg_data, pmic, NULL);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				pdev->name);
			ret = PTR_ERR(rdev);
			goto err_unregister_regulator;
		}

		/* Save regulator for cleanup */
		pmic->rdev[id] = rdev;
	}

	/* Start this loop from the id left from previous loop */
	for (; id < PALMAS_NUM_REGS; id++) {

		/* Miss out regulators which are not available due
		 * to alternate functions.
		 */

		reg_data = pdata->pmic_pdata->reg_data[id];

		/* Register the regulators */
		pmic->desc[id].name = palmas_regs_info[id].name;
		pmic->desc[id].id = id;
		pmic->desc[id].n_voltages = PALMAS_LDO_NUM_VOLTAGES;

		pmic->desc[id].ops = &palmas_ops_ldo;

		pmic->desc[id].type = REGULATOR_VOLTAGE;
		pmic->desc[id].owner = THIS_MODULE;

		rdev = regulator_register(&pmic->desc[id],
				&pdev->dev, reg_data, pmic, NULL);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				pdev->name);
			ret = PTR_ERR(rdev);
			goto err_unregister_regulator;
		}

		/* Save regulator for cleanup */
		pmic->rdev[id] = rdev;

		/* Initialise sleep/init values from platform data */
		if (pdata->pmic_pdata->reg_init) {
			reg_init = pdata->pmic_pdata->reg_init[id];
			if (reg_init) {
				ret = palmas_ldo_init(palmas, id, reg_init);
				if (ret)
					goto err_unregister_regulator;
			}
		}
	}

	return 0;

err_unregister_regulator:
	while (--id >= 0)
		regulator_unregister(pmic->rdev[id]);
	kfree(pmic->rdev);
err_free_desc:
	kfree(pmic->desc);
err_free_pmic:
	kfree(pmic);
	return ret;
}

static int __devexit palmas_remove(struct platform_device *pdev)
{
	struct palmas_pmic *pmic = platform_get_drvdata(pdev);
	int id;

	for (id = 0; id < PALMAS_NUM_REGS; id++)
		regulator_unregister(pmic->rdev[id]);

	kfree(pmic->rdev);
	kfree(pmic->desc);
	kfree(pmic);
	return 0;
}

static struct platform_driver palmas_driver = {
	.driver = {
		.name = "palmas-pmic",
		.owner = THIS_MODULE,
	},
	.probe = palmas_probe,
	.remove = __devexit_p(palmas_remove),
};

static int __init palmas_init(void)
{
	return platform_driver_register(&palmas_driver);
}
subsys_initcall(palmas_init);

static void __exit palmas_exit(void)
{
	platform_driver_unregister(&palmas_driver);
}
module_exit(palmas_exit);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:palmas-pmic");
