/*
 * bq27541_battery.c
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 * Donald Chan (hoiho@lab126.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/reboot.h>
#include <linux/timer.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>
#if defined(CONFIG_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include <linux/power/bq27541_battery.h>

#define DRIVER_NAME			"bq27541"
#define DRIVER_VERSION			"1.0"
#define DRIVER_AUTHOR			"Manish Lachwani"
#define BQ27541_I2C_ADDRESS		0x55	/* Battery I2C address on the bus */

/*
 * I2C registers that need to be read
 */
#define BQ27541_CONTROL			0x00
#define BQ27541_TEMP_LOW		0x06
#define BQ27541_TEMP_HI			0x07
#define BQ27541_VOLTAGE_LOW		0x08
#define BQ27541_VOLTAGE_HI		0x09
#define BQ27541_BATTERY_ID		0x7E
#define BQ27541_AI_LO			0x14	/* Average Current */
#define BQ27541_AI_HI			0x15
#define BQ27541_FLAGS_LO		0x0a
#define BQ27541_FLAGS_HI		0x0b
#define BQ27541_BATTERY_RESISTANCE	20
#define BQ27541_CSOC_L			0x2c	/* Compensated state of charge */
#define BQ27541_CSOC_H			0x2d
#define BQ27541_CAC_L			0x10	/* milliamp-hour */
#define BQ27541_CAC_H			0x11
#define BQ27541_FCC_L			0x12
#define BQ27541_FCC_H			0x13
#define BQ27541_AE_L			0x22
#define BQ27541_AE_H			0x23
#define BQ27541_CYCL_H			0x29
#define BQ27541_CYCL_L			0x28
#define BQ27541_FAC_H			0x0F
#define BQ27541_FAC_L			0x0E
#define BQ27541_NAC_H			0x0D
#define BQ27541_NAC_L			0x0C
#define BQ27541_CYCT_L			0x2A
#define BQ27541_CYCT_H			0x2B
#define BQ27541_TTE_L			0x16
#define BQ27541_TTE_H			0x17
#define BQ27541_DATA_FLASH_BLOCK	0x3f
#define BQ27541_MANUFACTURER_OFFSET	0x40	/* Offset for manufacturer ID */
#define BQ27541_MANUFACTURER_LENGTH	0x8	/* Length of manufacturer ID */

#define BQ27541_FLAGS_DSG		(1 << 0)
#define BQ27541_FLAGS_CHG		(1 << 8)
#define BQ27541_FLAGS_FC		(1 << 9)
#define BQ27541_FLAGS_OTD		(1 << 14)
#define BQ27541_FLAGS_OTC		(1 << 15)

#define BQ27541_TEMP_LOW_THRESHOLD	27	/* Low temperature threshold in 0.1 C */
#define BQ27541_TEMP_HI_THRESHOLD	450	/* High temperature threshold in 0.1 C */
#define BQ27541_TEMP_MID_THRESHOLD	100	/* Mid temperature threshold in 0.1 C */

#define BQ27541_VOLT_LOW_THRESHOLD	2500	/* Low voltage threshold in mV */
#define BQ27541_VOLT_HI_THRESHOLD	4350	/* High voltage threshold in mV */
#define BQ27541_VOLT_CRIT_THRESHOLD	3200	/* Critically low votlage threshold in mV */

#define BQ27541_BATTERY_INTERVAL	2000	/* 2 second duration */
#define BQ27541_BATTERY_INTERVAL_EARLY	1000	/* 1 second on probe */
#define BQ27541_BATTERY_INTERVAL_START	5000	/* 5 second timer on startup */
#define BQ27541_BATTERY_INTERVAL_ERROR	10000	/* 10 second timer after an error */

#define BQ27541_DEVICE_TYPE		0x0541	/* Device type of the gas gauge */

#define GENERAL_ERROR			0x0001
#define ID_ERROR			0x0002
#define TEMP_RANGE_ERROR		0x0004
#define VOLTAGE_ERROR			0x0008
#define DATA_CHANGED			0x0010

#define BQ27541_BATTERY_RETRY_THRESHOLD	5	/* Failed retry case - 5 */
#define BQ27541_ERROR_THRESHOLD		4	/* Max of 5 errors at most before sending to userspace */
#define BQ27541_BATTERY_RELAXED_THRESH	7200	/* Every 10 hours or 36000 seconds */

#define VBUS_DETECT			(1<<2)

int bq27541_reduce_charging = 0;
EXPORT_SYMBOL(bq27541_reduce_charging);

static int bq27541_lmd_counter = BQ27541_BATTERY_RELAXED_THRESH;

int bq27541_battery_tte = 0;
EXPORT_SYMBOL(bq27541_battery_tte);

struct bq27541_info {
	struct i2c_client	*client;
	struct device		*dev;
	struct bq27541_battery_platform_data *pdata;

	int battery_voltage;
	int battery_temperature;
	int battery_current;
	int battery_capacity;
	int battery_status;
	int battery_health;
	int battery_remaining_charge;
	int battery_remaining_charge_design;
	int battery_full_charge;
	int battery_full_charge_design;
	int battery_available_energy;
	int i2c_err;
	int err_flags;
	u8 manufacturer_id[BQ27541_MANUFACTURER_LENGTH + 1];

	struct power_supply	battery;
	struct delayed_work	battery_work;
	/* Time when system enters full suspend */
	struct timespec		suspend_time;
	/* Time when system enters early suspend */
	struct timespec		early_suspend_time;

	/* Battery capacity when system enters full suspend */
	int suspend_capacity;
	/* Battery capacity when system enters early suspend */
	int early_suspend_capacity;
#if defined(CONFIG_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};

static int bq27541_battery_lmd = 0;
static int bq27541_battery_fac = 0;
static int bq27541_battery_cycl = 0;
static int bq27541_battery_cyct = 0;

static int temp_error_counter = 0;
static int volt_error_counter = 0;

static int bq27541_i2c_read(struct bq27541_info *bat, u8 reg_num, void *value, int size) {
	s32 error;
	if (size == 1) {
		error = i2c_smbus_read_byte_data(bat->client, reg_num);
		if (error < 0) {
			dev_warn(bat->dev, "i2c read retry\n");
			return -EIO;
		}
		*((u8 *)value) = (u8)(error & 0xff);
		return 0;
	}
	else if (size == 2) {
		error = i2c_smbus_read_word_data(bat->client, reg_num);
		if (error < 0) {
			dev_warn(bat->dev, "i2c read retry\n");
			return -EIO;
		}
		*((u16 *)value) = (u16)(le16_to_cpu(error) & 0xffff);
		return 0;
	}
	else {
		dev_err(bat->dev, "Invalid data size: %d\n", size);
		return -1;
	}
}

static int bq27541_battery_read_u16(struct bq27541_info *bat, u8 reg_num, int *value) {
	u16 data = 0;
	int error = bq27541_i2c_read(bat, reg_num, &data, sizeof(data));
	if (!error)
		*value = data;
	return error;
}

static int bq27541_battery_read_voltage(struct bq27541_info *bat, int *voltage) {
	return bq27541_battery_read_u16(bat, BQ27541_VOLTAGE_LOW, voltage);
}
static int bq27541_battery_read_current(struct bq27541_info *bat, int *curr) {
	return bq27541_battery_read_u16(bat, BQ27541_AI_LO, curr);
}
static int bq27541_battery_read_capacity(struct bq27541_info *bat, int *capacity) {
	return bq27541_battery_read_u16(bat, BQ27541_CSOC_L, capacity);
}
static int bq27541_battery_read_flags(struct bq27541_info *bat, int *flags) {
	return bq27541_battery_read_u16(bat, BQ27541_FLAGS_LO, flags);
}
static int bq27541_battery_read_remaining_charge(struct bq27541_info *bat, int *mAh) {
	return bq27541_battery_read_u16(bat, BQ27541_CAC_L, mAh);
}
static int bq27541_battery_read_remaining_charge_design(struct bq27541_info *bat, int *mAh) {
	return bq27541_battery_read_u16(bat, BQ27541_NAC_L, mAh);
}
static int bq27541_battery_read_full_charge(struct bq27541_info *bat, int *mAh) {
	return bq27541_battery_read_u16(bat, BQ27541_FCC_L, mAh);
}
static int bq27541_battery_read_full_charge_design(struct bq27541_info *bat, int *mAh) {
	return bq27541_battery_read_u16(bat, BQ27541_FAC_L, mAh);
}
static int bq27541_battery_read_available_energy(struct bq27541_info *bat, int *energy) {
	return bq27541_battery_read_u16(bat, BQ27541_AE_L, energy);
}
static int bq27541_battery_read_tte(struct bq27541_info *bat, int *tte) {
	return bq27541_battery_read_u16(bat, BQ27541_TTE_L, tte);
}
static void bq27541_battery_read_lmd_cyc(struct bq27541_info *bat, int *lmd, int *cycl, int *cyct) {
	u16 value = 0;
	int error = bq27541_i2c_read(bat, BQ27541_CYCL_L, &value, sizeof(value));
	if (!error)
		*cycl = value;
}
static int bq27541_battery_read_fac(struct bq27541_info *bat, int *fac) {
	return bq27541_battery_read_u16(bat, BQ27541_FAC_L, fac);
}
static int bq27541_battery_read_temperature(struct bq27541_info *bat, int *temperature) {
	s16 temp = 0;
	int celsius = 0;
	int error = bq27541_i2c_read(bat, BQ27541_TEMP_LOW, &temp, sizeof(temp));
	if (!error) {
		/* Convert 0.1 K to 0.1 C */
		celsius = temp - 2732;
		*temperature = celsius;
	}
	return error;
}
static int bq27541_read_manufacturer_id(struct bq27541_info *bat) {
	const char unknown_str[8] = "UNKNOWN";
	static int unknown_flag = 0;

	/* Enable access to manufacturer info block A */
	i2c_smbus_write_byte_data(bat->client, BQ27541_DATA_FLASH_BLOCK, 1);
	msleep(10);

	memset(bat->manufacturer_id, 0, sizeof(bat->manufacturer_id));

	if (i2c_smbus_read_i2c_block_data(bat->client, BQ27541_MANUFACTURER_OFFSET, BQ27541_MANUFACTURER_LENGTH, bat->manufacturer_id) < 0) {
		dev_err(bat->dev, "Unable to get manufacturer ID\n");
		strncpy(bat->manufacturer_id, unknown_str, sizeof(unknown_str));
		unknown_flag = 1;
		return -1;
	}

#if defined(CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM)
	if (strstr(bat->manufacturer_id, "JEM") == NULL) {
		if (!unknown_flag) {
			dev_err(bat->dev, "Get wrong manufacturer ID: %s\n", bat->manufacturer_id);
			unknown_flag = 1;
		}
		memset(bat->manufacturer_id, 0, sizeof(bat->manufacturer_id));
		strncpy(bat->manufacturer_id, unknown_str, sizeof(unknown_str));
	} else if (unknown_flag)
			unknown_flag = 0;
#endif

	return 0;
}

/* Main battery timer task */
static void battery_handle_work(struct work_struct *work) {
	int err = 0;
	int batt_err_flag = 0;
	int value = 0, flags = 0;
	u8 vbus_value = 0;
	struct bq27541_info *bat = container_of(work, struct bq27541_info, battery_work.work);

	err = bq27541_battery_read_temperature(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_temperature != value) {
			bat->battery_temperature = value;
			batt_err_flag |= DATA_CHANGED;
		}
		bat->i2c_err = 0;
	}

	/*
	 * Check for the temperature range violation
	 */
	if ((bat->battery_temperature <= BQ27541_TEMP_LOW_THRESHOLD) || (bat->battery_temperature >= BQ27541_TEMP_HI_THRESHOLD)) {
		temp_error_counter++;
		bq27541_reduce_charging = 0;
	}
	else {
		if (bat->battery_temperature < BQ27541_TEMP_MID_THRESHOLD)
			bq27541_reduce_charging = 1;
		else
			bq27541_reduce_charging = 0;
		temp_error_counter = 0;
		bat->err_flags &= ~TEMP_RANGE_ERROR;
	}

	if (temp_error_counter > BQ27541_ERROR_THRESHOLD) {
		bat->err_flags |= TEMP_RANGE_ERROR;
		printk(KERN_ERR "battery driver temp - %d\n", bat->battery_temperature / 10);
		temp_error_counter = 0;
	}

	err = bq27541_battery_read_voltage(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_voltage != value) {
			bat->battery_voltage = value;
			batt_err_flag |= DATA_CHANGED;
		}
		bat->i2c_err = 0;
	}

	/* Check for critical battery voltage */
	if (bat->battery_voltage <= BQ27541_VOLT_CRIT_THRESHOLD) {
		printk(KERN_WARNING "bq27541: battery has reached critically low level, shutting down...\n");
		sys_sync();
		orderly_poweroff(true);
	}

	/* Check for the battery voltage range violation */
	if ((bat->battery_voltage <= BQ27541_VOLT_LOW_THRESHOLD) || (bat->battery_voltage >= BQ27541_VOLT_HI_THRESHOLD)) {
		volt_error_counter++;
	} else {
		volt_error_counter = 0;
		bat->err_flags &= ~VOLTAGE_ERROR;
	}
	if (volt_error_counter > BQ27541_ERROR_THRESHOLD) {
		dev_err(bat->dev, "battery driver voltage - %d mV\n", bat->battery_voltage);
		bat->err_flags |= VOLTAGE_ERROR;
		volt_error_counter = 0;
	}

	/* Check for the battery current */
	err = bq27541_battery_read_current(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;	
		goto out;
	} else {
		if (bat->battery_current != value) {
			bat->battery_current = value;
			batt_err_flag |= DATA_CHANGED;
		}
		bat->i2c_err = 0;
	}

	/* Read the gas gauge capacity */
	err = bq27541_battery_read_capacity(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_capacity != value) {
			bat->battery_capacity = value;
			batt_err_flag |= DATA_CHANGED;
		}
		bat->i2c_err = 0;
	}

	/*
	 * Check the battery status
	 */
	err = bq27541_battery_read_flags(bat, &flags);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		value = bat->battery_status;
		dev_dbg(bat->dev, "read old battery_status: %d\n", value);
		if ((flags & BQ27541_FLAGS_FC)
				&& (bat->battery_capacity == 100)
				&& (bat->battery_current == 0)) {
			dev_dbg(bat->dev, "battery_status set to POWER_SUPPLY_STATUS_FULL\n");
			value = POWER_SUPPLY_STATUS_FULL;
		} else if ((flags & BQ27541_FLAGS_DSG) || (bat->battery_current <= 0)) {
			dev_dbg(bat->dev, "battery_status set to POWER_SUPPLY_STATUS_DISCHARGING (flags & BQ27541_FLAGS_DSG) == %d, battery_current == %d\n", flags & BQ27541_FLAGS_DSG, bat->battery_current);
			value = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			dev_dbg(bat->dev, "battery_status set to POWER_SUPPLY_STATUS_CHARGING\n");
			value = POWER_SUPPLY_STATUS_CHARGING;
		}

		if (bat->battery_status != value) {
			dev_info(bat->dev, "battery_status CHANGED == %d\n", value);
			bat->battery_status = value;
			batt_err_flag |= DATA_CHANGED;
		}

		if (flags & (BQ27541_FLAGS_OTC | BQ27541_FLAGS_OTD)) {
			value = POWER_SUPPLY_HEALTH_OVERHEAT;
		} else {
			value = POWER_SUPPLY_HEALTH_GOOD;
		}

		if (bat->battery_health != value) {
			bat->battery_health = value;
			batt_err_flag |= DATA_CHANGED;
		}

		bat->i2c_err = 0;
	}

	/*
	 * Read the current battery mAH
	 */
	err = bq27541_battery_read_remaining_charge(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_remaining_charge != value) {
			bat->battery_remaining_charge = value;
			batt_err_flag |= DATA_CHANGED;
		}

		bat->i2c_err = 0;
	}

	/*
	 * Read the current battery mAH (uncompensated)
	 */
	err = bq27541_battery_read_remaining_charge_design(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_remaining_charge_design != value) {
			bat->battery_remaining_charge_design = value;
			batt_err_flag |= DATA_CHANGED;
		}

		bat->i2c_err = 0;
	}

	/*
	 * Read the manufacturer ID
	 */
	err = bq27541_read_manufacturer_id(bat);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		bat->i2c_err = 0;
	}

	/*
	 * Read the full battery mAH
	 */
	err = bq27541_battery_read_full_charge(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_full_charge != value) {
			bat->battery_full_charge = value;
			batt_err_flag |= DATA_CHANGED;
		}

		bat->i2c_err = 0;
	}

	/*
	 * Read the full battery mAH (uncompensated)
	 */
	err = bq27541_battery_read_full_charge_design(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_full_charge_design != value) {
			bat->battery_full_charge_design = value;
			batt_err_flag |= DATA_CHANGED;
		}

		bat->i2c_err = 0;
	}

	/*
	 * Read the available energy
	 */
	err = bq27541_battery_read_available_energy(bat, &value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (bat->battery_available_energy != value) {
			bat->battery_available_energy = value;
			batt_err_flag |= DATA_CHANGED;
		}

		bat->i2c_err = 0;
	}

	/* Take these readings every 10 hours */
	if (bq27541_lmd_counter == BQ27541_BATTERY_RELAXED_THRESH) {
		bq27541_lmd_counter = 0;
		bq27541_battery_read_lmd_cyc(bat, &bq27541_battery_lmd, &bq27541_battery_cycl, &bq27541_battery_cyct);
	}
	else {
		bq27541_lmd_counter++;
	}

	/* TTE readings */
	err =  bq27541_battery_read_tte(bat, &bq27541_battery_tte);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		bat->i2c_err = 0;
	}

	/* Full Available Capacity */
	err = bq27541_battery_read_fac(bat, &bq27541_battery_fac);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		bat->i2c_err = 0;
	}

out:
	if (batt_err_flag & GENERAL_ERROR) {
		if (++bat->i2c_err == BQ27541_BATTERY_RETRY_THRESHOLD) {
			printk(KERN_ERR "bq27541 battery: i2c read error, retry exceeded\n");
			bat->err_flags |= GENERAL_ERROR;
			bat->i2c_err = 0;
		}
	} else {
		bat->err_flags &= ~GENERAL_ERROR;
		bat->i2c_err = 0;
	}

	dev_dbg(bat->dev, "temp: %d, volt: %d, current: %d, capacity: %d%%, mAH: %d\n",
		bat->battery_temperature / 10, bat->battery_voltage,
		bat->battery_current, bat->battery_capacity,
		bat->battery_remaining_charge);

	/* Send uevent up if data has changed */
	if (batt_err_flag & DATA_CHANGED) {
		power_supply_changed(&bat->battery);

		/* handle LEDs */
		if ((bat->pdata->led_callback) && (!twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &vbus_value, 0x03))) {
		        if ((bat->battery_status == POWER_SUPPLY_STATUS_CHARGING) && (vbus_value & VBUS_DETECT)) {
				if (bat->battery_capacity < 90)
					bat->pdata->led_callback(0, 255);
				else
					bat->pdata->led_callback(255, 0);
		        }
			else if (bat->battery_status == POWER_SUPPLY_STATUS_FULL) {
				if (vbus_value & VBUS_DETECT)
					bat->pdata->led_callback(255, 0);
				else
					bat->pdata->led_callback(0, 0);
		        }
			else if (vbus_value & VBUS_DETECT)
				bat->pdata->led_callback(0, 0);
		}
	}

	if (bat->err_flags & GENERAL_ERROR) {
		/* Notify upper layers battery is dead */
		bat->battery_health = POWER_SUPPLY_HEALTH_UNKNOWN;
		power_supply_changed(&bat->battery);
	}

	if (batt_err_flag & GENERAL_ERROR) {
		schedule_delayed_work(&bat->battery_work,
			msecs_to_jiffies(BQ27541_BATTERY_INTERVAL_ERROR));
	} else {
		schedule_delayed_work(&bat->battery_work,
			msecs_to_jiffies(BQ27541_BATTERY_INTERVAL));
	}

	return;
}

static int bq27541_get_property(struct power_supply *ps,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	struct bq27541_info *bat = container_of(ps,
			struct bq27541_info, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bat->battery_status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* Convert mV to uV */
		val->intval = bat->battery_voltage * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Convert mA to uA */
		val->intval = bat->battery_current * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bat->battery_capacity;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bat->battery_temperature;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = bat->battery_health;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		/* Convert mAh to uAh */
		val->intval = bat->battery_remaining_charge * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW_DESIGN:
		/* Convert mAh to uAh */
		val->intval = bat->battery_remaining_charge_design * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		/* Convert mAh to uAh */
		val->intval = bat->battery_full_charge * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		/* Convert mAh to uAh */
		val->intval = bat->battery_full_charge_design * 1000;
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		/* Convert mW to uW */
		val->intval = bat->battery_available_energy * 1000;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = bat->manufacturer_id;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property bq27541_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	//POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	//POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_NOW_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_AVG,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

#if defined(CONFIG_EARLYSUSPEND)
static void bq27541_early_suspend(struct early_suspend *handler)
{
	struct bq27541_info *bat = container_of(handler,
			struct bq27541_info, early_suspend);

	/* Cache the current capacity */
	bat->early_suspend_capacity = bat->battery_capacity;

	/* Mark the suspend time */
	bat->early_suspend_time = current_kernel_time();
}

static void bq27541_late_resume(struct early_suspend *handler)
{
	struct bq27541_info *bat = container_of(handler, struct bq27541_info, early_suspend);
	int value = bat->battery_capacity;

	/* Compute elapsed time and determine screen off drainage */
	struct timespec diff = timespec_sub(current_kernel_time(),
			bat->early_suspend_time);

	if (bat->early_suspend_capacity != -1) {
		dev_info(bat->dev,
			"Screen off drainage: %d %% over %ld msecs\n",
			bat->early_suspend_capacity - value,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
	}
}
#endif

static int bq27541_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq27541_info *bat = NULL;
	int ret = 0;
	int dev_ver = 0;

	pr_info("%s: Entering probe...\n", DRIVER_NAME);

        client->addr = BQ27541_I2C_ADDRESS;

	if ((ret = i2c_smbus_write_word_data(client, BQ27541_CONTROL, 0x0001)) < 0) {
		pr_err("error writing device type: %d\n", ret);
		return -ENODEV;
	}

	dev_ver = i2c_smbus_read_word_data(client, BQ27541_CONTROL);

	if (dev_ver != BQ27541_DEVICE_TYPE) {
		pr_warning("%s: Failed to detect device type (%d), possibly drained battery?\n", DRIVER_NAME, dev_ver);
	}

        if (!(bat = kzalloc(sizeof(*bat), GFP_KERNEL))) {
                return -ENOMEM;
        }

        bat->client			= client;
	bat->dev			= &client->dev;
	bat->pdata			= (struct bq27541_battery_platform_data *)client->dev.platform_data;

	bat->battery.name		= "battery";
	bat->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	bat->battery.get_property	= bq27541_get_property;
	bat->battery.properties		= bq27541_battery_props;
	bat->battery.num_properties	= ARRAY_SIZE(bq27541_battery_props);

	/* Set some initial dummy values */
	bat->battery_capacity		= 5;
	bat->battery_voltage		= 3500;
	bat->battery_temperature	= 0;
	bat->battery_status		= POWER_SUPPLY_STATUS_UNKNOWN;

	bat->suspend_capacity		= -1;
	bat->early_suspend_capacity	= -1;

        i2c_set_clientdata(client, bat);

	bq27541_read_manufacturer_id(bat);

	ret = power_supply_register(&client->dev, &bat->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		i2c_set_clientdata(client, NULL);
		kfree(bat);
		return ret;
	}

	INIT_DELAYED_WORK(&bat->battery_work, battery_handle_work);

	schedule_delayed_work(&bat->battery_work, msecs_to_jiffies(BQ27541_BATTERY_INTERVAL_EARLY));

#if defined(CONFIG_EARLYSUSPEND)
	bat->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	bat->early_suspend.suspend = bq27541_early_suspend;
	bat->early_suspend.resume = bq27541_late_resume;
	register_early_suspend(&bat->early_suspend);
#endif

	dev_info(bat->dev, "probe succeeded\n");

        return 0;
}

static int bq27541_remove(struct i2c_client *client)
{
        struct bq27541_info *bat = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&bat->battery_work);

        i2c_set_clientdata(client, bat);

        return 0;
}

static void bq27541_shutdown(struct i2c_client *client)
{
	struct bq27541_info *bat = i2c_get_clientdata(client);
	cancel_delayed_work_sync(&bat->battery_work);
}

static int bq27541_battery_suspend(struct i2c_client *client, pm_message_t state)
{
        struct bq27541_info *bat = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&bat->battery_work);

	/* Cache the current capacity */
	bat->suspend_capacity = bat->battery_capacity;

	/* Mark the suspend time */
	bat->suspend_time = current_kernel_time();

	return 0;
}

static int bq27541_battery_resume(struct i2c_client *client)
{
	int err = 0, value = 0;
	struct bq27541_info *bat = i2c_get_clientdata(client);

	/* Check for the battery voltage */
	err += bq27541_battery_read_voltage(bat, &value);
	if (!err) {
		bat->battery_voltage = value;
		bat->i2c_err = 0;
	} else {
		bat->i2c_err++;
	}

	/* Check for remaining charge */
	err += bq27541_battery_read_remaining_charge(bat, &value);
	if (!err) {
		bat->battery_remaining_charge = value;
		bat->i2c_err = 0;
	} else {
		bat->i2c_err++;
	}

	/* Check for remaining charge (uncompensated) */
	err += bq27541_battery_read_remaining_charge_design(bat, &value);
	if (!err) {
		bat->battery_remaining_charge_design = value;
		bat->i2c_err = 0;
	} else {
		bat->i2c_err++;
	}

	/* Read the gas gauge capacity */
	err += bq27541_battery_read_capacity(bat, &value);
	if (!err) {
		bat->battery_capacity = value;
		bat->i2c_err = 0;
	} else {
		bat->i2c_err++;
	}

	/* Report to upper layers */
	power_supply_changed(&bat->battery);

	schedule_delayed_work(&bat->battery_work, msecs_to_jiffies(BQ27541_BATTERY_INTERVAL));

	return 0;
}

static const struct i2c_device_id bq27541_id[] =  {
        { "bq27541", 0 },
        { },
};
MODULE_DEVICE_TABLE(i2c, bq27541_id);

static unsigned short normal_i2c[] = { BQ27541_I2C_ADDRESS, I2C_CLIENT_END };

static struct i2c_driver bq27541_i2c_driver = {
	.driver = {
			.name = DRIVER_NAME,
		},
	.probe		= bq27541_probe,
	.remove		= bq27541_remove,
	.shutdown	= bq27541_shutdown,
	.suspend	= bq27541_battery_suspend,
	.resume		= bq27541_battery_resume,
	.id_table	= bq27541_id,
	.address_list	= normal_i2c,
};

static int __init bq27541_battery_init(void)
{
	int ret = i2c_add_driver(&bq27541_i2c_driver);
	if (ret) {
		printk(KERN_ERR "bq27541 battery: Could not add driver\n");
		return ret;
	}
	return 0;
}

static void __exit bq27541_battery_exit(void)
{
	i2c_del_driver(&bq27541_i2c_driver);
}

module_init(bq27541_battery_init);
module_exit(bq27541_battery_exit);

MODULE_DESCRIPTION("BQ27541 Battery Driver");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
