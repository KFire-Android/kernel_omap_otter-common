/*
 * BQ27x00 battery driver
 *
 * Copyright (C) 2008 Rodolfo Giometti <giometti@linux.it>
 * Copyright (C) 2008 Eurotech S.p.A. <info@eurotech.it>
 * Copyright (C) 2010-2011 Lars-Peter Clausen <lars@metafoo.de>
 * Copyright (C) 2011 Pali Rohár <pali.rohar@gmail.com>
 *
 * Based on a previous work by Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Datasheets:
 * http://focus.ti.com/docs/prod/folders/print/bq27000.html
 * http://focus.ti.com/docs/prod/folders/print/bq27500.html
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <plat/mux.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/thermal_framework.h>
#include <asm/unaligned.h>
#include <linux/mfd/palmas.h>

#include <linux/power/bq27x00_battery.h>

#define DRIVER_VERSION			"1.2.0"

#define BQ27x00_REG_CONTL		0x00 /* Control Register */
#define BQ27x00_REG_TEMP		0x06
#define BQ27x00_REG_VOLT		0x08
#define BQ27x00_REG_AI			0x14
#define BQ27x00_REG_FLAGS		0x0A
#define BQ27x00_REG_TTE			0x16
#define BQ27x00_REG_TTF			0x18
#define BQ27x00_REG_TTECP		0x26
#define BQ27x00_REG_NAC			0x0C /* Nominal available capacity */
#define BQ27x00_REG_LMD			0x12 /* Last measured discharge */
#define BQ27x00_REG_CYCT		0x2A /* Cycle count total */
#define BQ27x00_REG_AE			0x22 /* Available energy */

/* Control Commands */
#define BQ27x00_CONTL_CMD_CHRG_EN	0x001A /* Enable charging */
#define BQ27x00_CONTL_CMD_CHRG_DIS	0x001B /* Disable charging */

#define BQ27000_REG_RSOC		0x0B /* Relative State-of-Charge */
#define BQ27000_REG_ILMD		0x76 /* Initial last measured discharge */
#define BQ27000_FLAG_EDVF		BIT(0) /* Final End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_EDV1		BIT(1) /* First End-of-Discharge-Voltage flag */
#define BQ27000_FLAG_CI			BIT(4) /* Capacity Inaccurate flag */
#define BQ27000_FLAG_FC			BIT(5)
#define BQ27000_FLAG_CHGS		BIT(7) /* Charge state flag */

#define BQ27500_REG_SOC			0x2C
#define BQ27500_REG_DCAP		0x3C /* Design capacity */
#define BQ27500_FLAG_DSC		BIT(0)
#define BQ27500_FLAG_SOCF		BIT(1) /* State-of-Charge threshold final */
#define BQ27500_FLAG_SOC1		BIT(2) /* State-of-Charge threshold 1 */
#define BQ27500_FLAG_FC			BIT(9)

#define BQ27000_RS			20 /* Resistor sense */

/*
 * bq27530 controls bq24160 charger by its own dedicated i2c bus
 * The charger registers are mapped to a series of single byte
 * Charger Data Commands to enable system reading and writing of
 * battery charger registers.
 */
#define BQ24160_CHRGR_CTL_STAT_REG      0x76
#define BQ24160_CHRGR_CONTROL_REG	0x78

#define IUSB_LIMIT_2			BIT(6)
#define IUSB_LIMIT_1			BIT(5)
#define IUSB_LIMIT_0			BIT(4)
#define IUSB_LIMIT_MASK			(IUSB_LIMIT_2 | IUSB_LIMIT_1 | IUSB_LIMIT_0)

#define STAT_2				BIT(6)
#define STAT_1				BIT(5)
#define STAT_0				BIT(4)
#define STAT_MASK			(STAT_2 | STAT_1 | STAT_0)
#define AC_CHARGING			(STAT_1 | STAT_0)
#define AC_CHARGING_READY		STAT_0

struct bq27x00_device_info;
struct bq27x00_access_methods {
	int (*read)(struct bq27x00_device_info *di, u8 reg, bool single);
	int (*write)(struct bq27x00_device_info *di, u8 reg, u16 val,
							bool single);
};

enum bq27x00_chip { BQ27000, BQ27500, BQ27530 };

struct bq27x00_reg_cache {
	int temperature;
	int time_to_empty;
	int time_to_empty_avg;
	int time_to_full;
	int charge_full;
	int cycle_count;
	int capacity;
	int energy;
	int flags;
};

struct bq27x00_device_info {
	struct device 		*dev;
	int			id;
	int			gpio;
	int			gpio_irq;
	enum bq27x00_chip	chip;

	struct bq27x00_reg_cache cache;
	int charge_design_full;

	unsigned long last_update;
	struct delayed_work work;

	struct power_supply	bat;
	struct power_supply	ac;
	struct power_supply	usb;
	struct power_supply	bat_sim;

	struct bq27x00_access_methods bus;
	bool battery_present;
	int			charger_type;
	struct notifier_block   nb;

	struct	mutex lock;
	/* reference to the cooling device */
	struct	thermal_dev *tdev;
	bool enable_charger;
};

static enum power_supply_property bq27x00_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_ENERGY_NOW,
};

static unsigned int poll_interval = 1;

static enum power_supply_property bq27x00_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property bq27x00_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

module_param(poll_interval, uint, 0644);
MODULE_PARM_DESC(poll_interval, "battery poll interval in seconds - " \
				"0 disables polling");

/*
 * Common code for BQ27x00 devices
 */

static inline int bq27x00_read(struct bq27x00_device_info *di, u8 reg,
		bool single)
{
	return di->bus.read(di, reg, single);
}

static inline int bq27x00_write(struct bq27x00_device_info *di, u8 reg,
				u16 val, bool single)
{
	return di->bus.write(di, reg, val, single);
}

/*
 * Return the battery Relative State-of-Charge
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_rsoc(struct bq27x00_device_info *di)
{
	int rsoc;

	if ((di->chip == BQ27500) || (di->chip == BQ27530))
		rsoc = bq27x00_read(di, BQ27500_REG_SOC, false);
	else
		rsoc = bq27x00_read(di, BQ27000_REG_RSOC, true);

	if (rsoc < 0)
		dev_dbg(di->dev, "error reading relative State-of-Charge\n");

	return rsoc;
}

/*
 * Return a battery charge value in µAh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_charge(struct bq27x00_device_info *di, u8 reg)
{
	int charge;

	charge = bq27x00_read(di, reg, false);
	if (charge < 0) {
		dev_dbg(di->dev, "error reading charge register %02x: %d\n",
			reg, charge);
		return charge;
	}

	if ((di->chip == BQ27500) || (di->chip == BQ27530))
		charge *= 1000;
	else
		charge = charge * 3570 / BQ27000_RS;

	return charge;
}

/*
 * Return the battery Nominal available capaciy in µAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_nac(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_NAC);
}

/*
 * Return the battery Last measured discharge in µAh
 * Or < 0 if something fails.
 */
static inline int bq27x00_battery_read_lmd(struct bq27x00_device_info *di)
{
	return bq27x00_battery_read_charge(di, BQ27x00_REG_LMD);
}

/*
 * Return the battery Initial last measured discharge in µAh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_ilmd(struct bq27x00_device_info *di)
{
	int ilmd;

	if ((di->chip == BQ27500) || (di->chip == BQ27530))
		ilmd = bq27x00_read(di, BQ27500_REG_DCAP, false);
	else
		ilmd = bq27x00_read(di, BQ27000_REG_ILMD, true);

	if (ilmd < 0) {
		dev_dbg(di->dev, "error reading initial last measured discharge\n");
		return ilmd;
	}

	if ((di->chip == BQ27500) || (di->chip == BQ27530))
		ilmd *= 1000;
	else
		ilmd = ilmd * 256 * 3570 / BQ27000_RS;

	return ilmd;
}

/*
 * Return the battery Available energy in µWh
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_energy(struct bq27x00_device_info *di)
{
	int ae;

	ae = bq27x00_read(di, BQ27x00_REG_AE, false);
	if (ae < 0) {
		dev_dbg(di->dev, "error reading available energy\n");
		return ae;
	}

	if ((di->chip == BQ27500) || (di->chip == BQ27530))
		ae *= 1000;
	else
		ae = ae * 29200 / BQ27000_RS;

	return ae;
}

/*
 * Return the battery temperature in tenths of degree Celsius
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_temperature(struct bq27x00_device_info *di)
{
	int temp;

	temp = bq27x00_read(di, BQ27x00_REG_TEMP, false);
	if (temp < 0) {
		dev_err(di->dev, "error reading temperature\n");
		return temp;
	}

	if ((di->chip == BQ27500) || (di->chip == BQ27530))
		temp -= 2731;
	else
		temp = ((temp * 5) - 5463) / 2;

	return temp;
}

/*
 * Return the battery Cycle count total
 * Or < 0 if something fails.
 */
static int bq27x00_battery_read_cyct(struct bq27x00_device_info *di)
{
	int cyct;

	cyct = bq27x00_read(di, BQ27x00_REG_CYCT, false);
	if (cyct < 0)
		dev_err(di->dev, "error reading cycle count total\n");

	return cyct;
}

/*
 * Read a time register.
 * Return < 0 if something fails.
 */
static int bq27x00_battery_read_time(struct bq27x00_device_info *di, u8 reg)
{
	int tval;

	tval = bq27x00_read(di, reg, false);
	if (tval < 0) {
		dev_dbg(di->dev, "error reading time register %02x: %d\n",
			reg, tval);
		return tval;
	}

	if (tval == 65535)
		return -ENODATA;

	return tval * 60;
}

static void bq27x00_update(struct bq27x00_device_info *di)
{
	struct bq27x00_reg_cache cache = {0, };
	bool is_bq27500 = di->chip == BQ27500;
	bool is_bq27530 = di->chip == BQ27530;

	cache.flags = bq27x00_read(di, BQ27x00_REG_FLAGS, !(is_bq27500 || is_bq27530));
	if (cache.flags >= 0) {
		if (!(is_bq27500|| is_bq27530) && (cache.flags & BQ27000_FLAG_CI)) {
			dev_info(di->dev, "battery is not calibrated! ignoring capacity values\n");
			cache.capacity = -ENODATA;
			cache.energy = -ENODATA;
			cache.time_to_empty = -ENODATA;
			cache.time_to_empty_avg = -ENODATA;
			cache.time_to_full = -ENODATA;
			cache.charge_full = -ENODATA;
		} else {
			cache.capacity = bq27x00_battery_read_rsoc(di);
			cache.energy = bq27x00_battery_read_energy(di);
			cache.time_to_empty = bq27x00_battery_read_time(di, BQ27x00_REG_TTE);
			cache.time_to_empty_avg = bq27x00_battery_read_time(di, BQ27x00_REG_TTECP);
			if (di->chip != BQ27530)
				cache.time_to_full = bq27x00_battery_read_time(di, BQ27x00_REG_TTF);
			cache.charge_full = bq27x00_battery_read_lmd(di);
		}
		cache.temperature = bq27x00_battery_read_temperature(di);
		cache.cycle_count = bq27x00_battery_read_cyct(di);

		/* We only have to read charge design full once */
		if (di->charge_design_full <= 0)
			di->charge_design_full = bq27x00_battery_read_ilmd(di);
	}

	if (memcmp(&di->cache, &cache, sizeof(cache)) != 0) {
		di->cache = cache;
		power_supply_changed(&di->bat);
	}

	di->last_update = jiffies;
}

static void bq27x00_battery_poll(struct work_struct *work)
{
	struct bq27x00_device_info *di =
		container_of(work, struct bq27x00_device_info, work.work);

	bq27x00_update(di);

	if (poll_interval > 0) {
		/* The timer does not have to be accurate. */
		set_timer_slack(&di->work.timer, poll_interval * HZ / 4);
		schedule_delayed_work(&di->work, poll_interval * HZ);
	}
}

/*
 * Return the battery average current in µA
 * Note that current can be negative signed as well
 * Or 0 if something fails.
 */
static int bq27x00_battery_current(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int curr;
	int flags;

	curr = bq27x00_read(di, BQ27x00_REG_AI, false);
	if (curr < 0) {
		dev_err(di->dev, "error reading current\n");
		return curr;
	}

	if ((di->chip == BQ27500) || (di->chip == BQ27530)) {
		/* bq27500 returns signed value */
		val->intval = (int)((s16)curr) * 1000;
	} else {
		flags = bq27x00_read(di, BQ27x00_REG_FLAGS, false);
		if (flags & BQ27000_FLAG_CHGS) {
			dev_dbg(di->dev, "negative current!\n");
			curr = -curr;
		}

		val->intval = curr * 3570 / BQ27000_RS;
	}

	return 0;
}

static int bq27x00_battery_status(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int status;

	if ((di->chip == BQ27500) || (di->chip == BQ27530)) {
		if (di->cache.flags & BQ27500_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27500_FLAG_DSC)
			status = POWER_SUPPLY_STATUS_DISCHARGING;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		if (di->cache.flags & BQ27000_FLAG_FC)
			status = POWER_SUPPLY_STATUS_FULL;
		else if (di->cache.flags & BQ27000_FLAG_CHGS)
			status = POWER_SUPPLY_STATUS_CHARGING;
		else if (power_supply_am_i_supplied(&di->bat))
			status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else
			status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	val->intval = status;

	return 0;
}

static int bq27x00_battery_capacity_level(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int level;

	if ((di->chip == BQ27500) || (di->chip == BQ27530)) {
		if (di->cache.flags & BQ27500_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27500_FLAG_SOC1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27500_FLAG_SOCF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else {
		if (di->cache.flags & BQ27000_FLAG_FC)
			level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
		else if (di->cache.flags & BQ27000_FLAG_EDV1)
			level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
		else if (di->cache.flags & BQ27000_FLAG_EDVF)
			level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		else
			level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}

	val->intval = level;

	return 0;
}

/*
 * Return the battery Voltage in milivolts
 * Or < 0 if something fails.
 */
static int bq27x00_battery_voltage(struct bq27x00_device_info *di,
	union power_supply_propval *val)
{
	int volt;

	volt = bq27x00_read(di, BQ27x00_REG_VOLT, false);
	if (volt < 0) {
		dev_err(di->dev, "error reading voltage\n");
		return volt;
	}

	val->intval = volt * 1000;

	return 0;
}

static int bq27x00_simple_value(int value,
	union power_supply_propval *val)
{
	if (value < 0)
		return value;

	val->intval = value;

	return 0;
}

#define to_bq27x00_device_info(x) container_of((x), \
				struct bq27x00_device_info, bat);

static int bq27x00_battery_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	mutex_lock(&di->lock);
	if (time_is_before_jiffies(di->last_update + 5 * HZ)) {
		cancel_delayed_work_sync(&di->work);
		bq27x00_battery_poll(&di->work.work);
	}
	mutex_unlock(&di->lock);

	if (psp != POWER_SUPPLY_PROP_PRESENT && di->cache.flags < 0)
		return -ENODEV;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = bq27x00_battery_status(di, val);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27x00_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = di->cache.flags < 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = bq27x00_battery_current(di, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = bq27x00_simple_value(di->cache.capacity, val);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = bq27x00_battery_capacity_level(di, val);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = bq27x00_simple_value(di->cache.temperature, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_empty, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
		ret = bq27x00_simple_value(di->cache.time_to_empty_avg, val);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		ret = bq27x00_simple_value(di->cache.time_to_full, val);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = bq27x00_simple_value(bq27x00_battery_read_nac(di), val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = bq27x00_simple_value(di->cache.charge_full, val);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		ret = bq27x00_simple_value(di->charge_design_full, val);
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		ret = bq27x00_simple_value(di->cache.cycle_count, val);
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		ret = bq27x00_simple_value(di->cache.energy, val);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int bq27x00_bat_sim_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = 3800000;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_MAINS;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int bq27x00_ac_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di =
		container_of(psy, struct bq27x00_device_info, ac);
	u8 value;

	value = bq27x00_read(di, BQ24160_CHRGR_CTL_STAT_REG, false);
	value &= STAT_MASK;

	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = bq27x00_battery_voltage(di, val);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq27x00_battery_status(di, val);
		if (val->intval == POWER_SUPPLY_STATUS_CHARGING &&
		    (value == AC_CHARGING || value == AC_CHARGING_READY))
			val->intval = POWER_SUPPLY_TYPE_MAINS;
		else
			val->intval = POWER_SUPPLY_TYPE_UNKNOWN;

		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int bq27x00_usb_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	int ret = 0;
	struct bq27x00_device_info *di =
		container_of(psy, struct bq27x00_device_info, usb);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = di->charger_type;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static void bq27x00_external_power_changed(struct power_supply *psy)
{
	struct bq27x00_device_info *di = to_bq27x00_device_info(psy);

	cancel_delayed_work_sync(&di->work);
	schedule_delayed_work(&di->work, 0);
}

static irqreturn_t bq27x00_irq_handler(int irq, void *_priv)
{
	struct bq27x00_device_info *di = _priv;

	power_supply_changed(&di->ac);
	return IRQ_HANDLED;
}

static int bq27x00_powersupply_init(struct bq27x00_device_info *di)
{
	int ret;
	union power_supply_propval volt_val;
	union power_supply_propval curr_val;
	int status;
	/*
	 * Get the current consumption by battery. If it is 0mA
	 * or a small leaking current of 100mA either battery is
	 * not connected or battery is full
	 */
	ret = bq27x00_battery_current(di, &curr_val);
	if (ret) {
		dev_err(di->dev, "failed to get battery current: %d\n", ret);
		return ret;
	}

	if (abs(curr_val.intval) < 100000) {
		/*
		 * In case current is less than 100mA then check if
		 * voltage is more than 3.9V in that case we are
		 * supplying from battery as LDO is capable of
		 * supplying less than or equal to 3.7V
		 */
		ret = bq27x00_battery_voltage(di, &volt_val);
		if (ret) {
			dev_err(di->dev, "failed to get voltage: %d\n", ret);
			return ret;
		}
		/* Check if voltage is greater than 3.9v */
		if (volt_val.intval > 3900000)
			di->battery_present = true;
		else
			di->battery_present = false;
	} else {
		di->battery_present = true; /* Current is non-zero or no leak */
	}

	if (di->battery_present) {
		di->bat.type = POWER_SUPPLY_TYPE_BATTERY;
		di->bat.properties = bq27x00_battery_props;
		di->bat.num_properties = ARRAY_SIZE(bq27x00_battery_props);
		di->bat.get_property = bq27x00_battery_get_property;
		di->bat.external_power_changed = bq27x00_external_power_changed;

		INIT_DELAYED_WORK(&di->work, bq27x00_battery_poll);
		mutex_init(&di->lock);

		ret = power_supply_register(di->dev, &di->bat);
		if (ret) {
			dev_err(di->dev, "fail to register battery: %d\n", ret);
			return ret;
		}

		dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);

		di->ac.name = "ac-supply";
		di->ac.type = POWER_SUPPLY_TYPE_MAINS;
		di->ac.properties = bq27x00_ac_props;
		di->ac.num_properties = ARRAY_SIZE(bq27x00_ac_props);
		di->ac.get_property = bq27x00_ac_get_property;

		ret = power_supply_register(di->dev, &di->ac);
		if (ret) {
			dev_err(di->dev, "fail to register AC: %d\n", ret);
			power_supply_unregister(&di->bat);
			return ret;
		}

		di->usb.name = "usb-supply";
		di->usb.type = POWER_SUPPLY_TYPE_USB;
		di->usb.properties = bq27x00_usb_props;
		di->usb.num_properties = ARRAY_SIZE(bq27x00_usb_props);
		di->usb.get_property = bq27x00_usb_get_property;

		ret = power_supply_register(di->dev, &di->usb);
		if (ret) {
			dev_err(di->dev,
				"fail to register USB power supply: %d\n", ret);
			power_supply_unregister(&di->bat);
			return ret;
		}

		status = request_threaded_irq(di->gpio_irq, NULL,
				bq27x00_irq_handler,
				IRQF_TRIGGER_LOW, "bq27x00_soc_int",
				di);
		if (status) {
			dev_err(di->dev, "request irq failed for bq27x00_soc_int");
			power_supply_unregister(&di->bat);
			power_supply_unregister(&di->ac);
			return status;
		}

		bq27x00_update(di);
	} else {

		di->bat_sim.name = "bat-sim";
		di->bat_sim.type = POWER_SUPPLY_TYPE_MAINS;
		di->bat_sim.properties = bq27x00_ac_props;
		di->bat_sim.num_properties = ARRAY_SIZE(bq27x00_ac_props);
		di->bat_sim.get_property = bq27x00_bat_sim_get_property;

		ret = power_supply_register(di->dev, &di->bat_sim);
		if (ret) {
			dev_err(di->dev, "fail to register bat sim: %d\n", ret);
			return ret;
		}
		dev_info(di->dev, "support ver. %s enabled\n", DRIVER_VERSION);
	}
	return 0;
}

static void bq27x00_powersupply_unregister(struct bq27x00_device_info *di)
{
	/*
	 * power_supply_unregister call bq27x00_battery_get_property which
	 * call bq27x00_battery_poll.
	 * Make sure that bq27x00_battery_poll will not call
	 * schedule_delayed_work again after unregister (which cause OOPS).
	 */
	poll_interval = 0;

	cancel_delayed_work_sync(&di->work);

	power_supply_unregister(&di->bat);

	free_irq(di->gpio_irq, NULL);
	gpio_free(di->gpio);
	mutex_destroy(&di->lock);
}


/* i2c specific code */
#ifdef CONFIG_BATTERY_BQ27X00_I2C

/* If the system has several batteries we need a different name for each
 * of them...
 */
static DEFINE_IDR(battery_id);
static DEFINE_MUTEX(battery_mutex);

static int bq27x00_read_i2c(struct bq27x00_device_info *di, u8 reg, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[2];
	unsigned char data[2];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].buf = &reg;
	msg[0].len = sizeof(reg);
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;
	if (single)
		msg[1].len = 1;
	else
		msg[1].len = 2;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0)
		return ret;

	if (!single)
		ret = get_unaligned_le16(data);
	else
		ret = data[0];

	return ret;
}

static int
bq27x00_write_i2c(struct bq27x00_device_info *di, u8 reg, u16 val, bool single)
{
	struct i2c_client *client = to_i2c_client(di->dev);
	struct i2c_msg msg[1];
	unsigned char data[3];
	int ret;

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	data[0] = reg;
	data[1] = val & 0x00FF;
	if (!single) {
		data[2] = (val & 0xFF00) >> 8;
		msg[0].len = 3; /* 1 reg addr + 2 cmd bytes */
	} else {
		msg[0].len = 2; /* 1 reg addr + 1 cmd byte */
	}
	msg[0].buf = data;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));

	return ret;
}

static int bq27x00_usb_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct bq27x00_device_info *di =
		container_of(nb, struct bq27x00_device_info, nb);
	u8 val;

	di->charger_type = event;
	val = bq27x00_read_i2c(di, BQ24160_CHRGR_CONTROL_REG, false);
	val &= ~IUSB_LIMIT_MASK;

	switch (event) {
	case POWER_SUPPLY_TYPE_UNKNOWN:
	case POWER_SUPPLY_TYPE_BATTERY:
		/* on disconnect set safe minimal current - 100 mA */
		bq27x00_write(di, BQ24160_CHRGR_CONTROL_REG, val, true);
		break;
	case POWER_SUPPLY_TYPE_USB:
		/* USB2.0 host with 500 mA current limit */
		val |= IUSB_LIMIT_1;
		bq27x00_write(di, BQ24160_CHRGR_CONTROL_REG, val, true);
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_ACA:
		/* USB host/charger with 1500 mA current limit */
		val |= IUSB_LIMIT_2 | IUSB_LIMIT_0;
		bq27x00_write(di, BQ24160_CHRGR_CONTROL_REG, val, true);
	default:
		return NOTIFY_OK;
	}

	return NOTIFY_OK;
}

static int battery_apply_cooling(struct thermal_dev *dev,
				int level)
{
	struct i2c_client *client = to_i2c_client(dev->dev);
	struct bq27x00_device_info *di = i2c_get_clientdata(client);
	unsigned long charge_level;
	int percent;

	/* transform into percentage */
	percent = thermal_cooling_device_reduction_get(dev, level);
	if (percent < 0 || percent > 100)
		return -EINVAL;
	/*
	 * In the current design, only Charge ON/OFF is supported.
	 * Need to update the logic once more levels are identified
	 * for battery cooling device.
	 */
	dev_dbg(di->dev, "Cool levl %d percentage %d\n", level, percent);
	mutex_lock(&battery_mutex);
	if (percent) {
		/* Set the Battery Charging to ON */
		charge_level = 100;
		/* Control command is 0x001A */
		bq27x00_write(di, BQ27x00_REG_CONTL, BQ27x00_CONTL_CMD_CHRG_EN,
									false);
	} else {
		/* Set the Battery Charging to OFF */
		/* Control command is 0x001B */
		bq27x00_write(di, BQ27x00_REG_CONTL, BQ27x00_CONTL_CMD_CHRG_DIS,
									false);
		charge_level = 0;
	}
	mutex_unlock(&battery_mutex);

	return 0;
}

static int enable_charger_get(void *data, u64 *val)
{
	struct thermal_dev *tdev = data;
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct bq27x00_device_info *di = i2c_get_clientdata(client);

	*val = di->enable_charger;

	return 0;
}

static int enable_charger_set(void *data, u64 val)
{
	struct thermal_dev *tdev = data;
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct bq27x00_device_info *di = i2c_get_clientdata(client);

	mutex_lock(&battery_mutex);
	di->enable_charger = (int)val;

	if (di->enable_charger) {
		/* Set the Battery Charging to ON */
		/* Control command is 0x001A */
		bq27x00_write(di, BQ27x00_REG_CONTL, BQ27x00_CONTL_CMD_CHRG_EN,
									false);
	} else {
		/* Set the Battery Charging to OFF */
		/* Control command is 0x001B */
		bq27x00_write(di, BQ27x00_REG_CONTL, BQ27x00_CONTL_CMD_CHRG_DIS,
									false);
	}
	mutex_unlock(&battery_mutex);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(enable_charger_fops, enable_charger_get,
						enable_charger_set, "%llu\n");

#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
static int battery_register_debug_entries(struct thermal_dev *tdev,
					struct dentry *d)
{
	/* Read/Write - Debug properties of battery sensor */
	(void) debugfs_create_file("enable_charger",
			S_IRUGO | S_IWUSR, d, tdev,
			&enable_charger_fops);
	return 0;
}
#endif

static struct thermal_dev_ops battery_cooling_ops = {
	.cool_device = battery_apply_cooling,
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	.register_debug_entries = battery_register_debug_entries,
#endif
};

static struct thermal_dev battery_thermal_dev = {
	.name		= "battery_cooling",
	.domain_name	= "case",
	.dev_ops	= &battery_cooling_ops,
};

static int bq27x00_battery_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	char *name;
	struct bq27x00_device_info *di;
	struct bq27x00_platform_data *bq_pdata;
	struct thermal_dev *tdev;
	int num, i;
	int retval = 0;

	/* Get new ID for the new battery device */
	retval = idr_pre_get(&battery_id, GFP_KERNEL);
	if (retval == 0)
		return -ENOMEM;
	mutex_lock(&battery_mutex);
	retval = idr_get_new(&battery_id, client, &num);
	mutex_unlock(&battery_mutex);
	if (retval < 0)
		return retval;

	name = kasprintf(GFP_KERNEL, "%s-%d", id->name, num);
	if (!name) {
		dev_err(&client->dev, "failed to allocate device name\n");
		retval = -ENOMEM;
		goto batt_failed_1;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&client->dev, "failed to allocate device info data\n");
		retval = -ENOMEM;
		goto batt_failed_2;
	}

	di->id = num;
	di->dev = &client->dev;
	di->chip = id->driver_data;
	di->bat.name = name;
	di->bus.read = &bq27x00_read_i2c;
	di->bus.write = &bq27x00_write_i2c;
	di->gpio = client->irq;
	retval = gpio_request_one(di->gpio, GPIOF_IN, "bq_gpio");
	if (retval < 0) {
		dev_err(di->dev, "Could not request for GPIO:%i\n",
				di->gpio);
		goto batt_failed_3;
	}

	di->enable_charger = true;
	di->gpio_irq = gpio_to_irq(di->gpio);

	if (bq27x00_powersupply_init(di))
		goto batt_failed_4;

	i2c_set_clientdata(client, di);

	di->nb.notifier_call = bq27x00_usb_notifier_call;
	palmas_usb_register_notifier(&di->nb);

	/* Register Battery as cooling device for Case domain */
	if (di->battery_present) {
		bq_pdata = dev_get_platdata(&client->dev);
		/*
		 * If there is no thermal cooling info for the battery, then
		 * battery won't participate in the thermal policy as a cooling
		 * device. But battery should be allowed to operate normally.
		 * Hence skipping the thermal registration but allowing probe
		 * function to succeed.
		 */
		if (!bq_pdata) {
			dev_err(&client->dev, "%s: Invalid platform data\n",
								__func__);
			goto no_thermal_control;
		}

		tdev = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
		if (!tdev) {
			dev_err(&client->dev, "failed to allocate thermal data\n");
			retval = -ENOMEM;
			goto batt_failed_4;
		}

		memcpy(tdev, &battery_thermal_dev, sizeof(struct thermal_dev));
		tdev->dev = di->dev;
		di->tdev = tdev;

		retval = thermal_cooling_dev_register(tdev);
		if (retval < 0) {
			dev_err(&client->dev, "failed to register thermal device\n");
			goto batt_failed_5;
		}

		/* Add the cooling actions for Battery cooling device */
		for (i = 0; i < bq_pdata->number_actions; i++)
			thermal_insert_cooling_action(di->tdev,
				bq_pdata->cooling_actions[i].priority,
				bq_pdata->cooling_actions[i].percentage);

	} else {
		dev_info(&client->dev, "Not in Battery mode\n");
	}

no_thermal_control:

	return 0;

batt_failed_5:
	kfree(tdev);
batt_failed_4:
	gpio_free(di->gpio);
batt_failed_3:
	kfree(di);
batt_failed_2:
	kfree(name);
batt_failed_1:
	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, num);
	mutex_unlock(&battery_mutex);

	return retval;
}

static int bq27x00_battery_remove(struct i2c_client *client)
{
	struct bq27x00_device_info *di = i2c_get_clientdata(client);

	bq27x00_powersupply_unregister(di);

	if (di->tdev) {
		thermal_cooling_dev_unregister(di->tdev);
		kfree(di->tdev);
	}

	kfree(di->bat.name);

	mutex_lock(&battery_mutex);
	idr_remove(&battery_id, di->id);
	mutex_unlock(&battery_mutex);

	kfree(di);

	return 0;
}

static const struct i2c_device_id bq27x00_id[] = {
	{ "bq27200", BQ27000 },	/* bq27200 is same as bq27000, but with i2c */
	{ "bq27500", BQ27500 },
	{ "bq27530", BQ27530 },
	{},
};
MODULE_DEVICE_TABLE(i2c, bq27x00_id);

static struct i2c_driver bq27x00_battery_driver = {
	.driver = {
		.name = "bq27x00-battery",
	},
	.probe = bq27x00_battery_probe,
	.remove = bq27x00_battery_remove,
	.id_table = bq27x00_id,
};

static inline int bq27x00_battery_i2c_init(void)
{
	int ret = i2c_add_driver(&bq27x00_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27x00 i2c driver\n");

	return ret;
}

static inline void bq27x00_battery_i2c_exit(void)
{
	i2c_del_driver(&bq27x00_battery_driver);
}

#else

static inline int bq27x00_battery_i2c_init(void) { return 0; }
static inline void bq27x00_battery_i2c_exit(void) {};

#endif

/* platform specific code */
#ifdef CONFIG_BATTERY_BQ27X00_PLATFORM

static int bq27000_read_platform(struct bq27x00_device_info *di, u8 reg,
			bool single)
{
	struct device *dev = di->dev;
	struct bq27000_platform_data *pdata = dev->platform_data;
	unsigned int timeout = 3;
	int upper, lower;
	int temp;

	if (!single) {
		/* Make sure the value has not changed in between reading the
		 * lower and the upper part */
		upper = pdata->read(dev, reg + 1);
		do {
			temp = upper;
			if (upper < 0)
				return upper;

			lower = pdata->read(dev, reg);
			if (lower < 0)
				return lower;

			upper = pdata->read(dev, reg + 1);
		} while (temp != upper && --timeout);

		if (timeout == 0)
			return -EIO;

		return (upper << 8) | lower;
	}

	return pdata->read(dev, reg);
}

static int __devinit bq27000_battery_probe(struct platform_device *pdev)
{
	struct bq27x00_device_info *di;
	struct bq27000_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform_data supplied\n");
		return -EINVAL;
	}

	if (!pdata->read) {
		dev_err(&pdev->dev, "no hdq read callback supplied\n");
		return -EINVAL;
	}

	di = kzalloc(sizeof(*di), GFP_KERNEL);
	if (!di) {
		dev_err(&pdev->dev, "failed to allocate device info data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, di);

	di->dev = &pdev->dev;
	di->chip = BQ27000;

	di->bat.name = pdata->name ?: dev_name(&pdev->dev);
	di->bus.read = &bq27000_read_platform;

	ret = bq27x00_powersupply_init(di);
	if (ret)
		goto err_free;

	return 0;

err_free:
	platform_set_drvdata(pdev, NULL);
	kfree(di);

	return ret;
}

static int __devexit bq27000_battery_remove(struct platform_device *pdev)
{
	struct bq27x00_device_info *di = platform_get_drvdata(pdev);

	bq27x00_powersupply_unregister(di);

	platform_set_drvdata(pdev, NULL);
	kfree(di);

	return 0;
}

static struct platform_driver bq27000_battery_driver = {
	.probe	= bq27000_battery_probe,
	.remove = __devexit_p(bq27000_battery_remove),
	.driver = {
		.name = "bq27000-battery",
		.owner = THIS_MODULE,
	},
};

static inline int bq27x00_battery_platform_init(void)
{
	int ret = platform_driver_register(&bq27000_battery_driver);
	if (ret)
		printk(KERN_ERR "Unable to register BQ27000 platform driver\n");

	return ret;
}

static inline void bq27x00_battery_platform_exit(void)
{
	platform_driver_unregister(&bq27000_battery_driver);
}

#else

static inline int bq27x00_battery_platform_init(void) { return 0; }
static inline void bq27x00_battery_platform_exit(void) {};

#endif

/*
 * Module stuff
 */

static int __init bq27x00_battery_init(void)
{
	int ret;

	ret = bq27x00_battery_i2c_init();
	if (ret)
		return ret;

	ret = bq27x00_battery_platform_init();
	if (ret)
		bq27x00_battery_i2c_exit();

	return ret;
}
module_init(bq27x00_battery_init);

static void __exit bq27x00_battery_exit(void)
{
	bq27x00_battery_platform_exit();
	bq27x00_battery_i2c_exit();
}
module_exit(bq27x00_battery_exit);

MODULE_AUTHOR("Rodolfo Giometti <giometti@linux.it>");
MODULE_DESCRIPTION("BQ27x00 battery monitor driver");
MODULE_LICENSE("GPL");
