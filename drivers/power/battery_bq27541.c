/*
 * bq27541_battery.c
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 * Donald Chan (hoiho@lab126.com)
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
#include <linux/usb/omap_usb.h>
#if defined(CONFIG_AMAZON_METRICS_LOG)
#include <linux/metricslog.h>
#endif
#if defined(CONFIG_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

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

#define BQ27541_I2C_ADDRESS		0x55	/* Battery I2C address on the bus */
#define BQ27541_TEMP_LOW_THRESHOLD	27	/* Low temperature threshold in 0.1 C */
#define BQ27541_TEMP_HI_THRESHOLD	450	/* High temperature threshold in 0.1 C */
#define BQ27541_TEMP_MID_THRESHOLD	100	/* Mid temperature threshold in 0.1 C */
#define BQ27541_VOLT_LOW_THRESHOLD	2500	/* Low voltage threshold in mV */
#define BQ27541_VOLT_HI_THRESHOLD	4350	/* High voltage threshold in mV */
#define BQ27541_VOLT_CRIT_THRESHOLD	3200	/* Critically low votlage threshold in mV */
/* Critically low voltage threshold in mV on suspend */
#define BQ27541_VOLT_CRIT_SUSPEND_THRESHOLD	3300
#define BQ27541_BATTERY_INTERVAL	2000	/* 2 second duration */
#define BQ27541_BATTERY_INTERVAL_START	5000	/* 5 second timer on startup */
#define BQ27541_BATTERY_INTERVAL_ERROR	10000	/* 10 second timer after an error */
#define BQ27541_DEVICE_TYPE		0x0541	/* Device type of the gas gauge */

#define DRIVER_NAME			"bq27541"
#define DRIVER_VERSION			"1.0"
#define DRIVER_AUTHOR			"Manish Lachwani"

#define GENERAL_ERROR			0x0001
#define ID_ERROR			0x0002
#define TEMP_RANGE_ERROR		0x0004
#define VOLTAGE_ERROR			0x0008
#define DATA_CHANGED			0x0010

#define BQ27541_BATTERY_RETRY_THRESHOLD	5	/* Failed retry case - 5 */

#define BQ27541_ERROR_THRESHOLD		4	/* Max of 5 errors at most before sending to userspace */

#define BQ27541_BATTERY_RELAXED_THRESH	7200	/* Every 10 hours or 36000 seconds */

int bq27541_reduce_charging = 0;
EXPORT_SYMBOL(bq27541_reduce_charging);

static int bq27541_lmd_counter = BQ27541_BATTERY_RELAXED_THRESH;

int bq27541_battery_tte = 0;

EXPORT_SYMBOL(bq27541_battery_tte);

enum BQ27541_FLAGS {
	BQ27541_STATUS_VALID = 0x1,
	BQ27541_STATUS_RESUMING = 0x2,
	BQ27541_STATUS_INIT_DONE = 0x4,
};

struct bq27541_info {
	struct mutex lock;
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
	int flags;
	u8 manufacturer_id[BQ27541_MANUFACTURER_LENGTH + 1];
	struct i2c_client *client;
	struct power_supply battery;
	struct delayed_work battery_work;
	struct workqueue_struct *work_queue;
	/* Time when system enters full suspend */
	struct timespec suspend_time;
	/* Time when system enters early suspend */
	struct timespec early_suspend_time;
	/* Battery capacity when system enters full suspend */
	int suspend_capacity;
	/* Battery capacity when system enters early suspend */
	int early_suspend_capacity;
#if defined(CONFIG_AMAZON_METRICS_LOG) && defined(CONFIG_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct notifier_block nb;
};

static int bq27541_battery_lmd = 0;
static int bq27541_battery_fac = 0;
static int bq27541_battery_cycl = 0;
static int bq27541_battery_cyct = 0;

static int temp_error_counter = 0;
static int volt_error_counter = 0;

static struct i2c_client *bq27541_battery_i2c_client;

static int bq27541_i2c_read(u8 reg_num, void *value, int size)
{
	s32 error;

	if (size == 1) {
		error = i2c_smbus_read_byte_data(bq27541_battery_i2c_client, reg_num);

		if (error < 0) {
			dev_warn(&bq27541_battery_i2c_client->dev,
					"i2c read retry\n");
			return -EIO;
		}

		*((u8 *)value) = (u8)(error & 0xff);

		return 0;
	} else if (size == 2) {
		error = i2c_smbus_read_word_data(bq27541_battery_i2c_client, reg_num);

		if (error < 0) {
			dev_warn(&bq27541_battery_i2c_client->dev,
					"i2c read retry\n");
			return -EIO;
		}

		*((u16 *)value) = (u16)(le16_to_cpu(error) & 0xffff);

		return 0;
	} else {
		dev_err(&bq27541_battery_i2c_client->dev,
				"Invalid data size: %d\n", size);
		return -1;
	}
}

static int bq27541_battery_read_voltage(int *voltage)
{
	u16 volts = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_VOLTAGE_LOW, &volts, sizeof(volts));

	if (!error)
		*voltage = volts;

	return error;
}

static int bq27541_battery_read_current(int *curr)
{
	s16 c = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_AI_LO, &c, sizeof(c));

	if (!error)
		*curr = c;

	return error;
}

static int bq27541_battery_read_capacity(int *capacity)
{
	u16 c = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_CSOC_L, &c, sizeof(c));

	if (!error)
		*capacity = c;

	return error;
}

static int bq27541_battery_read_flags(int *flags)
{
	u16 f = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_FLAGS_LO, &f, sizeof(f));

	if (!error)
		*flags = f;

	return error;
}

static int bq27541_battery_read_remaining_charge(int *mAh)
{
	u16 charge = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_CAC_L, &charge, sizeof(charge));

	if (!error)
		*mAh = charge;

	return error;
}

static int bq27541_battery_read_remaining_charge_design(int *mAh)
{
	u16 charge = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_NAC_L, &charge, sizeof(charge));

	if (!error)
		*mAh = charge;

	return error;
}

static int bq27541_battery_read_full_charge(int *mAh)
{
	u16 charge = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_FCC_L, &charge, sizeof(charge));

	if (!error)
		*mAh = charge;

	return error;
}

static int bq27541_battery_read_full_charge_design(int *mAh)
{
	u16 charge = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_FAC_L, &charge, sizeof(charge));

	if (!error)
		*mAh = charge;

	return error;
}


static int bq27541_battery_read_available_energy(int *energy)
{
	u16 ae = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_AE_L, &ae, sizeof(ae));

	if (!error)
		*energy = ae;

	return error;
}

/* Read TTE */
static int bq27541_battery_read_tte(int *tte)
{
	u16 value = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_TTE_L, &value, sizeof(value));

	if (!error)
		*tte = value;

	return error;
}

/* Read Last Measured Discharge and Learning count */
static void bq27541_battery_read_lmd_cyc(int *lmd, int *cycl, int *cyct)
{
	u16 value = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_CYCL_L, &value, sizeof(value));

	if (!error)
		*cycl = value;
}

/* Read Full Available Capacity */
static int bq27541_battery_read_fac(int *fac)
{
	u16 value = 0;
	int error = 0;

	error = bq27541_i2c_read(BQ27541_FAC_L, &value, sizeof(value));

	if (!error)
		*fac = value;

	return error;
}

static int bq27541_battery_read_temperature(int *temperature)
{
	s16 temp = 0;
	int celsius = 0, error = 0;

	error = bq27541_i2c_read(BQ27541_TEMP_LOW, &temp, sizeof(temp));

	if (!error) {
		/* Convert 0.1 K to 0.1 C */
		celsius = temp - 2732;
		*temperature = celsius;
	}

	return error;
}

static int bq27541_read_manufacturer_id(struct bq27541_info *info)
{
	const char unknown_str[8] = "UNKNOWN";
	static int unknown_flag = 0;

	/* Enable access to manufacturer info block A */
	i2c_smbus_write_byte_data(info->client, BQ27541_DATA_FLASH_BLOCK, 1);
	msleep(10);

	memset(info->manufacturer_id, 0, sizeof(info->manufacturer_id));

	if (i2c_smbus_read_i2c_block_data(info->client,
			BQ27541_MANUFACTURER_OFFSET,
			BQ27541_MANUFACTURER_LENGTH,
			info->manufacturer_id) < 0) {
		printk(KERN_WARNING "bq27541: Unable to get manufacturer ID\n");
		strncpy(info->manufacturer_id, unknown_str, sizeof(unknown_str));
		unknown_flag = 1;
		return -1;
	}

#if defined(CONFIG_MACH_OMAP4_BOWSER_SUBTYPE_JEM)
	if (strstr(info->manufacturer_id, "JEM") == NULL) {
		if (!unknown_flag) {
			printk(KERN_WARNING "bq27541: Get wrong manufacturer ID: %s\n", info->manufacturer_id);
			unknown_flag = 1;
		}
		memset(info->manufacturer_id, 0, sizeof(info->manufacturer_id));
		strncpy(info->manufacturer_id, unknown_str, sizeof(unknown_str));
	} else {
		if (unknown_flag)
			unknown_flag = 0;
	}
#endif

	return 0;
}

static void
battery_critical_voltage_check(struct bq27541_info *info, uint32_t value)
{
	if (info->battery_voltage <= value) {
		printk(KERN_WARNING
			"bq27541: battery has reached critically low level, "
			"shutting down...\n");

#ifdef CONFIG_AMAZON_METRICS_LOG
		{
			char buf[128];
			snprintf(buf, sizeof(buf),
				"bq27541:def:critical_shutdown=1;CT;1:HI");

			log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
		}
#endif
		sys_sync();
		orderly_poweroff(true);
	}
}

#if defined(CONFIG_AMAZON_METRICS_LOG)
/* must be called with info->lock held */
static void
battery_metrics_locked(struct bq27541_info *info)
{
	struct timespec diff;
	diff = timespec_sub(current_kernel_time(), info->suspend_time);

	if ((info->flags & BQ27541_STATUS_VALID) &&
		 info->suspend_capacity != -1) {
		char buf[256];
		snprintf(buf, sizeof(buf),
			"suspend_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
			info->suspend_capacity - info->battery_capacity,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
		log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);
		dev_info(&bq27541_battery_i2c_client->dev,
			"Suspend drainage: %d %% over %ld msecs\n",
			info->suspend_capacity - info->battery_capacity,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);

		snprintf(buf, sizeof(buf),
			"batt:def:cap=%d;CT;1,mv=%d;CT;1,current_avg=%d;CT;1,"
			"temp_g=%d;CT;1,charge=%d,charge_design=%d;CT;1:NR",
			info->battery_capacity, info->battery_voltage,
			info->battery_current, info->battery_temperature,
			info->battery_remaining_charge,
			info->battery_remaining_charge_design);
		log_to_metrics(ANDROID_LOG_INFO, "bq27541", buf);
		dev_info(&bq27541_battery_i2c_client->dev, "%s\n", buf);
	}
}
#endif

/* Main battery timer task */
static void battery_handle_work(struct work_struct *work)
{
	int err = 0;
	int batt_err_flag = 0;
	int value = 0, flags = 0;
	struct bq27541_info *info = container_of(work,
				struct bq27541_info, battery_work.work);

	mutex_lock(&info->lock);

	err = bq27541_battery_read_temperature(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_temperature != value ||
		    !(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_temperature = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Check for the temperature range violation
	 */
	if ( (info->battery_temperature <= BQ27541_TEMP_LOW_THRESHOLD) ||
		(info->battery_temperature >= BQ27541_TEMP_HI_THRESHOLD) ) {
			temp_error_counter++;
			bq27541_reduce_charging = 0;
	}
	else {
		if (info->battery_temperature < BQ27541_TEMP_MID_THRESHOLD)
			bq27541_reduce_charging = 1;
		else
			bq27541_reduce_charging = 0;

		temp_error_counter = 0;
		info->err_flags &= ~TEMP_RANGE_ERROR;
	}

	if (temp_error_counter > BQ27541_ERROR_THRESHOLD) {
		info->err_flags |= TEMP_RANGE_ERROR;
		printk(KERN_ERR "battery driver temp - %d\n",
				info->battery_temperature / 10);
		temp_error_counter = 0;
	}

	err = bq27541_battery_read_voltage(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_voltage != value ||
		    !(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_voltage = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/* Check for critical battery voltage */
	battery_critical_voltage_check(info, BQ27541_VOLT_CRIT_THRESHOLD);

	/*
	 * Check for the battery voltage range violation
	 */
	if ( (info->battery_voltage <= BQ27541_VOLT_LOW_THRESHOLD) ||
		(info->battery_voltage >= BQ27541_VOLT_HI_THRESHOLD) ) {
			volt_error_counter++;
	} else {
		volt_error_counter = 0;
		info->err_flags &= ~VOLTAGE_ERROR;
	}

	if (volt_error_counter > BQ27541_ERROR_THRESHOLD) {
		printk(KERN_ERR "battery driver voltage - %d mV\n",
				info->battery_voltage);
		info->err_flags |= VOLTAGE_ERROR;
		volt_error_counter = 0;
	}

	/*
	 * Check for the battery current
	 */
	err = bq27541_battery_read_current(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_current != value ||
		    !(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_current = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Read the gas gauge capacity
	 */
	err = bq27541_battery_read_capacity(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_capacity != value ||
		    !(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_capacity = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Check the battery status
	 */
	err = bq27541_battery_read_flags(&flags);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		value = info->battery_status;
		if ((flags & BQ27541_FLAGS_FC)
				&& (info->battery_capacity == 100)
				&& (info->battery_current == 0)) {
			value = POWER_SUPPLY_STATUS_FULL;
		} else if ((flags & BQ27541_FLAGS_DSG) || (info->battery_current <= 0)) {
			value = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			value = POWER_SUPPLY_STATUS_CHARGING;
		}

		if (info->battery_status != value ||
			!(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_status = value;
			batt_err_flag |= DATA_CHANGED;
		}

		if (flags & (BQ27541_FLAGS_OTC | BQ27541_FLAGS_OTD)) {
			value = POWER_SUPPLY_HEALTH_OVERHEAT;
		} else {
			value = POWER_SUPPLY_HEALTH_GOOD;
		}

		if (info->battery_health != value ||
			!(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_health = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Read the current battery mAH
	 */
	err = bq27541_battery_read_remaining_charge(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_remaining_charge != value ||
			!(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_remaining_charge = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Read the current battery mAH (uncompensated)
	 */
	err = bq27541_battery_read_remaining_charge_design(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_remaining_charge_design != value ||
			!(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_remaining_charge_design = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Read the manufacturer ID
	 */
	err = bq27541_read_manufacturer_id(info);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		info->i2c_err = 0;
	}

	/*
	 * Read the full battery mAH
	 */
	err = bq27541_battery_read_full_charge(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_full_charge != value ||
			!(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_full_charge = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Read the full battery mAH (uncompensated)
	 */
	err = bq27541_battery_read_full_charge_design(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_full_charge_design != value ||
			!(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_full_charge_design = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/*
	 * Read the available energy
	 */
	err = bq27541_battery_read_available_energy(&value);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		if (info->battery_available_energy != value ||
			!(info->flags & BQ27541_STATUS_VALID)) {
			info->battery_available_energy = value;
			batt_err_flag |= DATA_CHANGED;
		}

		info->i2c_err = 0;
	}

	/* Take these readings every 10 hours */
	if (bq27541_lmd_counter == BQ27541_BATTERY_RELAXED_THRESH) {
		bq27541_lmd_counter = 0;
		bq27541_battery_read_lmd_cyc(&bq27541_battery_lmd, &bq27541_battery_cycl, &bq27541_battery_cyct);
	}
	else {
		bq27541_lmd_counter++;
	}

	/* TTE readings */
	err =  bq27541_battery_read_tte(&bq27541_battery_tte);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		info->i2c_err = 0;
	}

	/* Full Available Capacity */
	err = bq27541_battery_read_fac(&bq27541_battery_fac);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	} else {
		info->i2c_err = 0;
	}

	/* all the properties are valid; ok to update status */
	info->flags |= BQ27541_STATUS_VALID;

#if defined(CONFIG_AMAZON_METRICS_LOG)
	if ((info->flags & BQ27541_STATUS_RESUMING)) {
		info->flags &= ~BQ27541_STATUS_RESUMING;
		battery_metrics_locked(info);
	}
#endif

out:
	if (batt_err_flag & GENERAL_ERROR) {
		if (++info->i2c_err == BQ27541_BATTERY_RETRY_THRESHOLD) {
			printk(KERN_ERR "bq27541 battery: i2c read error, retry exceeded\n");
			info->err_flags |= GENERAL_ERROR;
			info->i2c_err = 0;
		}
	} else {
		info->err_flags &= ~GENERAL_ERROR;
		info->i2c_err = 0;
	}

	pr_debug("temp: %d, volt: %d, current: %d, capacity: %d%%, mAH: %d\n",
		info->battery_temperature / 10, info->battery_voltage,
		info->battery_current, info->battery_capacity,
		info->battery_remaining_charge);

	/* Send uevent up if data has changed */
	if ((batt_err_flag & DATA_CHANGED) &&
		(info->flags & BQ27541_STATUS_VALID))
		power_supply_changed(&info->battery);

	if (info->err_flags & GENERAL_ERROR) {
		/* Notify upper layers battery is dead
		 * force update of all values next time */
		info->flags &= ~BQ27541_STATUS_VALID;
		info->battery_health = POWER_SUPPLY_HEALTH_UNKNOWN;
		power_supply_changed(&info->battery);
	}

	mutex_unlock(&info->lock);

	if (batt_err_flag & GENERAL_ERROR) {
		queue_delayed_work(info->work_queue, &info->battery_work,
			msecs_to_jiffies(BQ27541_BATTERY_INTERVAL_ERROR));
	} else {
		queue_delayed_work(info->work_queue, &info->battery_work,
			msecs_to_jiffies(BQ27541_BATTERY_INTERVAL));
	}

	return;
}

static int twl6030_usb_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct bq27541_info *info = container_of(nb, struct bq27541_info, nb);
	int value = info->battery_status;
#ifdef CONFIG_AMAZON_METRICS_LOG
	int cap = info->battery_capacity;
	char buf[128];
#endif

	switch (event) {
	case TWL6030_USB_EVENT_VBUS_ON:
		value = POWER_SUPPLY_STATUS_CHARGING;
		cancel_delayed_work_sync(&info->battery_work);
		queue_delayed_work(info->work_queue, &info->battery_work,
			msecs_to_jiffies(BQ27541_BATTERY_INTERVAL_START));

#ifdef CONFIG_AMAZON_METRICS_LOG
		snprintf(buf, sizeof(buf),
			"bq27541:def:POWER_STATUS_CHARGING=1;CT;1,"
			"cap=%u;CT;1:NR", cap);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
#endif

		break;
	case TWL6030_USB_EVENT_VBUS_OFF:
		value = POWER_SUPPLY_STATUS_DISCHARGING;
		cancel_delayed_work_sync(&info->battery_work);
		queue_delayed_work(info->work_queue, &info->battery_work,
			msecs_to_jiffies(BQ27541_BATTERY_INTERVAL));

#ifdef CONFIG_AMAZON_METRICS_LOG
		snprintf(buf, sizeof(buf),
			"bq27541:def:POWER_STATUS_DISCHARGING=1;CT;1,"
			"cap=%u;CT;1:NR", cap);

		log_to_metrics(ANDROID_LOG_INFO, "battery", buf);
#endif

		break;
	default:
		break;
	}

	if (value != info->battery_status) {
		info->battery_status = value;
		/* Send uevent up if data has changed */
		power_supply_changed(&info->battery);
	}

	return NOTIFY_DONE;
}


static const struct i2c_device_id bq27541_id[] =  {
	{ "bq27541", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, bq27541_id);

static int bq27541_get_property(struct power_supply *ps,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;
	int retry_cnt = 3;
	struct bq27541_info *info = container_of(ps,
			struct bq27541_info, battery);

retry:
	mutex_lock(&info->lock);
	dev_dbg(&info->client->dev, "flags=%08X\n", info->flags);

	if (!(ACCESS_ONCE(info->flags) & BQ27541_STATUS_INIT_DONE)) {
		mutex_unlock(&info->lock);
		dev_dbg(&info->client->dev,
			"get_property failed in init: flags=%08X\n",
			info->flags);
		return -EBUSY;
	}

	if (!(ACCESS_ONCE(info->flags) & BQ27541_STATUS_VALID)) {
		mutex_unlock(&info->lock);
		queue_delayed_work(info->work_queue, &info->battery_work, 0);
		if (!retry_cnt)
			return -EAGAIN;
		dev_dbg(&info->client->dev,
			"get_property waiting: flags=%08X\n", info->flags);
		msleep(20);
		retry_cnt--;
		goto retry;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = info->battery_status;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* Convert mV to uV */
		val->intval = info->battery_voltage * 1000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Convert mA to uA */
		val->intval = info->battery_current * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = info->battery_capacity;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = info->battery_temperature;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->battery_health;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		/* Convert mAh to uAh */
		val->intval = info->battery_remaining_charge * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW_DESIGN:
		/* Convert mAh to uAh */
		val->intval = info->battery_remaining_charge_design * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		/* Convert mAh to uAh */
		val->intval = info->battery_full_charge * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		/* Convert mAh to uAh */
		val->intval = info->battery_full_charge_design * 1000;
		break;
	case POWER_SUPPLY_PROP_ENERGY_AVG:
		/* Convert mW to uW */
		val->intval = info->battery_available_energy * 1000;
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = info->manufacturer_id;
		break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&info->lock);

	return ret;
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

#if defined(CONFIG_AMAZON_METRICS_LOG) && defined(CONFIG_EARLYSUSPEND)
static void bq27541_log_metrics(struct bq27541_info *info, char *msg,
	char *metricsmsg)
{
	int value = info->battery_capacity;
	struct timespec curr = current_kernel_time();
	/* Compute elapsed time and determine screen off or on drainage */
	struct timespec diff = timespec_sub(curr,
			info->early_suspend_time);

	if (info->early_suspend_capacity != -1) {
		char buf[512];
		snprintf(buf, sizeof(buf),
			"%s:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
			metricsmsg,
			info->early_suspend_capacity - value,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
		log_to_metrics(ANDROID_LOG_INFO, "drain_metrics", buf);
		dev_info(&bq27541_battery_i2c_client->dev,
			"%s: %d %% over %ld msecs\n",
			msg,
			info->early_suspend_capacity - value,
			diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC);
	}
	/* Cache the current capacity */
	info->early_suspend_capacity = info->battery_capacity;
	/* Mark the suspend or resume time */
	info->early_suspend_time = curr;
}

static void bq27541_early_suspend(struct early_suspend *handler)
{
	struct bq27541_info *info = container_of(handler,
			struct bq27541_info, early_suspend);

	bq27541_log_metrics(info, "Screen on drainage", "screen_on_drain");

}

static void bq27541_late_resume(struct early_suspend *handler)
{
	struct bq27541_info *info = container_of(handler,
			struct bq27541_info, early_suspend);
	bq27541_log_metrics(info, "Screen off drainage", "screen_off_drain");
}
#endif

static int bq27541_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bq27541_info *info = NULL;
	int ret = 0;
	int dev_ver = 0;

	pr_info("%s: Entering probe...\n", DRIVER_NAME);

	bq27541_battery_i2c_client = client;
	bq27541_battery_i2c_client->addr = BQ27541_I2C_ADDRESS;

	if ((ret = i2c_smbus_write_word_data(client,
				BQ27541_CONTROL, 0x0001)) < 0) {
		pr_err("error writing device type: %d\n", ret);
		bq27541_battery_i2c_client = NULL;
		return -ENODEV;
	}

	dev_ver = i2c_smbus_read_word_data(client, BQ27541_CONTROL);

	if (dev_ver != BQ27541_DEVICE_TYPE) {
		pr_warning("%s: Failed to detect device type (%d), "
				"possibly drained battery?\n",
				DRIVER_NAME, dev_ver);
	}

	if (!(info = kzalloc(sizeof(*info), GFP_KERNEL))) {
		bq27541_battery_i2c_client = NULL;
		return -ENOMEM;
	}

	mutex_init(&info->lock);

	client->addr = BQ27541_I2C_ADDRESS;

	i2c_set_clientdata(client, info);

	info->client = client;
	info->battery.name = "bq27541";
	info->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	info->battery.get_property = bq27541_get_property;
	info->battery.properties = bq27541_battery_props;
	info->battery.num_properties = ARRAY_SIZE(bq27541_battery_props);

	/* Set some initial dummy values */
	info->battery_capacity = 100;
	info->battery_voltage = 3500;
	info->battery_temperature = 0;
	info->battery_status = POWER_SUPPLY_STATUS_UNKNOWN;

	info->suspend_capacity = -1;
	info->early_suspend_capacity = -1;

	bq27541_read_manufacturer_id(info);

	info->work_queue = create_singlethread_workqueue("bq27541_wq");
	if (!info->work_queue) {
		dev_err(&client->dev, "failed to create a workqueue\n");
		ret = -ENOMEM;
		goto err1;
	}
	INIT_DELAYED_WORK(&info->battery_work, battery_handle_work);

	ret = power_supply_register(&client->dev, &info->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto err1;
	}

	info->nb.notifier_call = twl6030_usb_notifier_call;
	ret = twl6030_usb_register_notifier(&info->nb);
	if (ret) {
		dev_err(&client->dev,
			"twl6030 usb register notifier"
			" failed %d\n", ret);
		goto errq;
	}

#if defined(CONFIG_AMAZON_METRICS_LOG) && defined(CONFIG_EARLYSUSPEND)
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	info->early_suspend.suspend = bq27541_early_suspend;
	info->early_suspend.resume = bq27541_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	mutex_lock(&info->lock);
	info->flags |= BQ27541_STATUS_INIT_DONE;
	mutex_unlock(&info->lock);
	queue_delayed_work(info->work_queue, &info->battery_work, 0);

	pr_info("%s: probe succeeded\n", DRIVER_NAME);

	return 0;
errq:
	destroy_workqueue(info->work_queue);
err1:
	i2c_set_clientdata(client, NULL);
	kfree(info);

	return ret;
}

static int bq27541_remove(struct i2c_client *client)
{
	struct bq27541_info *info = i2c_get_clientdata(client);

	twl6030_usb_unregister_notifier(&info->nb);

	cancel_delayed_work_sync(&info->battery_work);
	destroy_workqueue(info->work_queue);

	i2c_set_clientdata(client, info);

	return 0;
}

static void bq27541_shutdown(struct i2c_client *client)
{
	struct bq27541_info *info = i2c_get_clientdata(client);

	twl6030_usb_unregister_notifier(&info->nb);

	cancel_delayed_work_sync(&info->battery_work);
}

static int bq27541_battery_suspend(struct i2c_client *client, pm_message_t state)
{
	struct bq27541_info *info = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&info->battery_work);

	/* Cache the current capacity */
	info->suspend_capacity = info->battery_capacity;
	battery_critical_voltage_check(info,
			BQ27541_VOLT_CRIT_SUSPEND_THRESHOLD);

	/* Mark the suspend time */
	info->suspend_time = current_kernel_time();

	return 0;
}

static int bq27541_battery_resume(struct i2c_client *client)
{
	struct bq27541_info *info = i2c_get_clientdata(client);

	/* invalidate all the measurements */
	mutex_lock(&info->lock);
	info->flags |= BQ27541_STATUS_RESUMING;
	info->flags &= ~BQ27541_STATUS_VALID;
	mutex_unlock(&info->lock);
	queue_delayed_work(info->work_queue, &info->battery_work, 0);

	return 0;
}

static unsigned short normal_i2c[] = { BQ27541_I2C_ADDRESS, I2C_CLIENT_END };

static struct i2c_driver bq27541_i2c_driver = {
	.driver = {
			.name = DRIVER_NAME,
		},
	.probe = bq27541_probe,
	.remove = bq27541_remove,
	.shutdown = bq27541_shutdown,
	.suspend = bq27541_battery_suspend,
	.resume = bq27541_battery_resume,
	.id_table = bq27541_id,
	.address_list = normal_i2c,
};

static int __init bq27541_battery_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&bq27541_i2c_driver);

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
