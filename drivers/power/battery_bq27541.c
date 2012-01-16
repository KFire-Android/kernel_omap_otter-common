/*
 * bq27541_battery.c
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/sysdev.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/i2c/twl.h>

/*
 * I2C registers that need to be read
 */
#define BQ27541_TEMP_LOW		0x06
#define BQ27541_TEMP_HI			0x07
#define BQ27541_VOLTAGE_LOW		0x08
#define BQ27541_VOLTAGE_HI		0x09
#define BQ27541_BATTERY_ID		0x7E
#define BQ27541_AI_LO			0x14	/* Average Current */
#define BQ27541_AI_HI			0x15
#define BQ27541_FLAGS			0x0a
#define BQ27541_BATTERY_RESISTANCE	20
#define BQ27541_CSOC_L			0x2c	/* Compensated state of charge */
#define BQ27541_CSOC_H			0x2d
#define BQ27541_CAC_L			0x10	/* milliamp-hour */
#define BQ27541_CAC_H			0x11
#define BQ27541_CYCL_H			0x29
#define BQ27541_CYCL_L			0x28
#define BQ27541_FAC_H			0x0F
#define BQ27541_FAC_L			0x0E
#define BQ27541_CYCT_L			0x2A
#define BQ27541_CYCT_H			0x2B
#define BQ27541_TTE_L			0x16
#define BQ27541_TTE_H			0x17

#define BQ27541_I2C_ADDRESS		0x55	/* Battery I2C address on the bus */
#define BQ27541_TEMP_LOW_THRESHOLD	37
#define BQ27541_TEMP_HI_THRESHOLD	113
#define BQ27541_TEMP_MID_THRESHOLD	50
#define BQ27541_VOLT_LOW_THRESHOLD	2500
#define BQ27541_VOLT_HI_THRESHOLD	4350
#define BQ27541_BATTERY_INTERVAL	20000	/* 20 second duration */
#define BQ27541_BATTERY_INTERVAL_EARLY	1000	/* 1 second on probe */
#define BQ27541_BATTERY_INTERVAL_START	5000	/* 5 second timer on startup */
#define BQ27541_BATTERY_INTERVAL_ERROR	10000	/* 10 second timer after an error */

#define DRIVER_NAME			"bq27541_Battery"
#define DRIVER_VERSION			"1.0"
#define DRIVER_AUTHOR			"Manish Lachwani"

#define GENERAL_ERROR			0x0001
#define ID_ERROR			0x0002
#define TEMP_RANGE_ERROR		0x0004
#define VOLTAGE_ERROR			0x0008

#define BQ27541_BATTERY_RETRY_THRESHOLD	5	/* Failed retry case - 5 */

#define BQ27541_ERROR_THRESHOLD		4	/* Max of 5 errors at most before sending to userspace */

#define BQ27541_BATTERY_RELAXED_THRESH	1800	/* Every 10 hours or 36000 seconds */
static int bq27541_battery_no_stats = 1;

unsigned int bq27541_battery_error_flags = 0;
EXPORT_SYMBOL(bq27541_battery_error_flags);


int bq27541_reduce_charging = 0;
EXPORT_SYMBOL(bq27541_reduce_charging);

static int bq27541_lmd_counter = BQ27541_BATTERY_RELAXED_THRESH;

int bq27541_battery_id = 0;
int bq27541_voltage = 0;
int bq27541_temperature = 0;
int bq27541_battery_current = 0;
int bq27541_battery_capacity = 0;
int bq27541_battery_mAH = 0;
int bq27541_battery_tte = 0;

EXPORT_SYMBOL(bq27541_battery_id);
EXPORT_SYMBOL(bq27541_voltage);
EXPORT_SYMBOL(bq27541_temperature);
EXPORT_SYMBOL(bq27541_battery_current);
EXPORT_SYMBOL(bq27541_battery_capacity);
EXPORT_SYMBOL(bq27541_battery_mAH);
EXPORT_SYMBOL(bq27541_battery_tte);

struct bq27541_info {
        struct i2c_client *client;
	struct power_supply battery;
};

static int bq27541_battery_lmd = 0;
static int bq27541_battery_fac = 0;
static int bq27541_battery_cycl = 0;
static int bq27541_battery_cyct = 0;

static int i2c_error_counter = 0;	/* Retry counter */
static int battery_driver_stopped = 0;	/* Battery stop/start flag */
static int last_suspend_current = 0;	/* Last suspend gasguage reading */

static int temp_error_counter = 0;
static int volt_error_counter = 0;

static int battery_curr_diags = 0;
static int battery_suspend_curr_diags = 0;

static int bq27541_battreg = 0;
static struct i2c_client *bq27541_battery_i2c_client;

/*
 * Create the sysfs entries
 */

static ssize_t
battery_id_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_battery_id);
}
static SYSDEV_ATTR(battery_id, 0644, battery_id_show, NULL);

static ssize_t
battery_current_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_battery_current);
}
static SYSDEV_ATTR(battery_current, 0644, battery_current_show, NULL);

static ssize_t
battery_suspend_current_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", last_suspend_current);
}
static SYSDEV_ATTR(battery_suspend_current, 0644, battery_suspend_current_show, NULL);

static ssize_t
battery_voltage_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_voltage);
}
static SYSDEV_ATTR(battery_voltage, 0644, battery_voltage_show, NULL);

static ssize_t
battery_temperature_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_temperature);
}
static SYSDEV_ATTR(battery_temperature, 0644, battery_temperature_show, NULL);

static ssize_t
battery_capacity_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d%%\n", bq27541_battery_capacity);
}
static SYSDEV_ATTR(battery_capacity, 0644, battery_capacity_show, NULL);

static ssize_t
battery_mAH_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_battery_mAH);
}
static SYSDEV_ATTR(battery_mAH, 0644, battery_mAH_show, NULL);

static ssize_t
battery_error_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_battery_error_flags);
}
static SYSDEV_ATTR(battery_error, 0644, battery_error_show, NULL);

static ssize_t
battery_current_diags_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", battery_curr_diags);
}
static SYSDEV_ATTR(battery_current_diags, 0644, battery_current_diags_show, NULL);

static ssize_t
battery_suspend_current_diags_show(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", battery_suspend_curr_diags);
}
static SYSDEV_ATTR(battery_suspend_current_diags, 0644, battery_suspend_current_diags_show, NULL);

/* This is useful for runtime debugging */

static ssize_t
battery_show_temp_thresholds(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "Hi-%dF, Lo-%dF\n", BQ27541_TEMP_LOW_THRESHOLD, BQ27541_TEMP_HI_THRESHOLD);
}
static SYSDEV_ATTR(battery_temp_thresholds, 0644, battery_show_temp_thresholds, NULL);

static ssize_t
battery_show_voltage_thresholds(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "Hi-%dmV, Lo-%dmV\n", BQ27541_VOLT_LOW_THRESHOLD, BQ27541_VOLT_HI_THRESHOLD);
}
static SYSDEV_ATTR(battery_voltage_thresholds, 0644, battery_show_voltage_thresholds, NULL);

static ssize_t
battery_show_lmd(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_battery_lmd);
}
static SYSDEV_ATTR(battery_lmd, 0644, battery_show_lmd, NULL);

static ssize_t
battery_show_cycl(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", bq27541_battery_cycl);
}
static SYSDEV_ATTR(battery_cycl, 0644, battery_show_cycl, NULL);

static ssize_t
battery_show_cyct(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", bq27541_battery_cyct);
}
static SYSDEV_ATTR(battery_cyct, 0644, battery_show_cyct, NULL);

static ssize_t
battery_show_polling_intervals(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "Regular-%dms, Bootup-%dms\n", 
				BQ27541_BATTERY_INTERVAL, BQ27541_BATTERY_INTERVAL_START);
}
static SYSDEV_ATTR(battery_polling_intervals, 0644, battery_show_polling_intervals, NULL);

static ssize_t
battery_show_i2c_address(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "i2c address-0x%x\n", BQ27541_I2C_ADDRESS);
}
static SYSDEV_ATTR(battery_i2c_address, 0644, battery_show_i2c_address, NULL);

static ssize_t
battery_show_resume_stats(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_battery_no_stats);
}

static ssize_t
battery_store_resume_stats(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t count)
{
	int value;

	if ((sscanf(buf, "%d", &value) > 0) &&
		((value == 0) || (value == 1))) {
			bq27541_battery_no_stats = value;
			return strlen(buf);
	}

	return -EINVAL;
}
static SYSDEV_ATTR(resume_stats, S_IRUGO|S_IWUSR, battery_show_resume_stats, battery_store_resume_stats);

static ssize_t
battery_show_battreg(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bq27541_battreg);
}

static ssize_t
battery_store_battreg(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t count)
{
	int value;

	if (sscanf(buf, "%x", &value) <= 0) {
		printk(KERN_ERR "Could not store the battery register value\n");
		return -EINVAL;
	}

	bq27541_battreg = value;
	return count;
}

static SYSDEV_ATTR(battreg, S_IRUGO|S_IWUSR, battery_show_battreg, battery_store_battreg);

static ssize_t
battery_show_battreg_value(struct sys_device *dev, struct sysdev_attribute *attr, char *buf)
{
	int value = 0;
	char *curr = buf;

	value = i2c_smbus_read_byte_data(bq27541_battery_i2c_client, bq27541_battreg);
	curr += sprintf(curr, "Battery Register %d\n", bq27541_battreg);
	curr += sprintf(curr, "\n");
	curr += sprintf(curr, " Value: 0x%x\n", value);
	curr += sprintf(curr, "\n");
	
	return curr - buf;
}

static ssize_t
battery_store_battreg_value(struct sys_device *dev, struct sysdev_attribute *attr, const char *buf, size_t count)
{
	int value;
	u8 tmp = 0;

	if (sscanf(buf, "%x", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}

	tmp = (u8)value;
	i2c_smbus_write_byte_data(bq27541_battery_i2c_client, bq27541_battreg, tmp);

	return count;
}

static SYSDEV_ATTR(battreg_value, S_IRUGO|S_IWUSR, battery_show_battreg_value, battery_store_battreg_value);
	
static struct sysdev_class bq27541_battery_sysclass = {
	.name = "bq27541_battery",
};

static struct sys_device bq27541_battery_device = {
	.id = 0,
	.cls = &bq27541_battery_sysclass,
};

static int bq27541_battery_sysdev_ctrl_init(void)
{
	int err = 0;

	err = sysdev_class_register(&bq27541_battery_sysclass);
	if (!err)
		err = sysdev_register(&bq27541_battery_device);
	if (!err) {
		sysdev_create_file(&bq27541_battery_device, &attr_battery_id);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_current);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_voltage);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_temperature);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_capacity);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_mAH);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_voltage_thresholds);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_polling_intervals);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_temp_thresholds);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_i2c_address);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_error);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_suspend_current);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_current_diags);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_suspend_current_diags);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_cycl);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_lmd);
		sysdev_create_file(&bq27541_battery_device, &attr_battery_cyct);
		sysdev_create_file(&bq27541_battery_device, &attr_resume_stats);
		sysdev_create_file(&bq27541_battery_device, &attr_battreg);
		sysdev_create_file(&bq27541_battery_device, &attr_battreg_value);
	}

	return err;
}

static void bq27541_battery_sysdev_ctrl_exit(void)
{
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_id);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_current);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_voltage);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_temperature);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_capacity);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_mAH);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_voltage_thresholds);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_polling_intervals);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_temp_thresholds);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_i2c_address);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_error);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_suspend_current);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_current_diags);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_suspend_current_diags);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_cycl);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_lmd);
	sysdev_remove_file(&bq27541_battery_device, &attr_battery_cyct);
	sysdev_remove_file(&bq27541_battery_device, &attr_resume_stats);
	sysdev_remove_file(&bq27541_battery_device, &attr_battreg);
	sysdev_remove_file(&bq27541_battery_device, &attr_battreg_value);

	sysdev_unregister(&bq27541_battery_device);
	sysdev_class_unregister(&bq27541_battery_sysclass);
}

int bq27541_battery_present = 0;
EXPORT_SYMBOL(bq27541_battery_present);

static int bq27541_i2c_read(unsigned char reg_num, unsigned char *value)
{
	s32 error;

	error = i2c_smbus_read_byte_data(bq27541_battery_i2c_client, reg_num);
	if (error < 0) {
		printk(KERN_INFO "bq27541_battery: i2c read retry\n");
		return -EIO;
	}

	*value = (unsigned char) (error & 0xFF);
	return 0;
}

static int bq27541_battery_read_voltage(int *voltage)
{
	unsigned char hi, lo;
	int volts;
	int err1 = 0, err2 = 0;
	int error = 0;
	
	err1 = bq27541_i2c_read(BQ27541_VOLTAGE_LOW, &lo);
	err2 = bq27541_i2c_read(BQ27541_VOLTAGE_HI, &hi);

	error = err1 | err2;

	if (!error) {
		volts = (hi << 8) | lo;
		*voltage = volts;
	}
	
	return error;
}

static int bq27541_battery_read_current(int *curr)
{
	unsigned char hi, lo, flags;
	int c;
	int err1 = 0, err2 = 0, err3 = 0;
	int sign = 1;
	int error = 0;

	err1 = bq27541_i2c_read(BQ27541_AI_LO, &lo);
	err2 = bq27541_i2c_read(BQ27541_AI_HI, &hi);
	err3 = bq27541_i2c_read(BQ27541_FLAGS, &flags);

	error = err1 | err2 | err3;

	if (!error) {
		if (flags & 0x80)
			sign = 1;
		else
			sign = -1;

		battery_curr_diags = sign * (((hi << 8) | lo) * 357);

		if (!battery_suspend_curr_diags)
			battery_suspend_curr_diags = battery_curr_diags;

		c = sign * ((((hi << 8) | lo) * 357) / (100 * BQ27541_BATTERY_RESISTANCE));
		*curr = c;
	}

	return error;
}

static int bq27541_battery_read_capacity(int *capacity)
{
	unsigned char hi, lo;
	int err1 = 0, err2 = 0;
	int error = 0;

	err1 = bq27541_i2c_read(BQ27541_CSOC_L, &lo);
	err2 = bq27541_i2c_read(BQ27541_CSOC_H, &hi);

	error = err1 | err2;

	if (!error)
		*capacity = (hi << 8) | lo;

	return error;
}
	
static int bq27541_battery_read_mAH(int *mAH)
{
	unsigned char hi, lo;
	int err1 = 0, err2 = 0;
	int error = 0;
	
	err1 = bq27541_i2c_read(BQ27541_CAC_L, &lo);
	err2 = bq27541_i2c_read(BQ27541_CAC_H, &hi);

	error = err1 | err2;

	if (!error)
		*mAH = ((((hi << 8) | lo) * 357) / (100 * BQ27541_BATTERY_RESISTANCE));
	
	return error;
}

/* Read TTE */
static int bq27541_battery_read_tte(int *tte)
{
	unsigned char hi, lo;
	int err1 = 0, err2 = 0;
	int error = 0;

	hi = lo = 0;

	err1 = bq27541_i2c_read(BQ27541_TTE_L, &lo);
	err2 = bq27541_i2c_read(BQ27541_TTE_H, &hi);

	error = err1 | err2;

	if (!error)
		*tte = (hi << 8) | lo;

	return error;
}

/* Read Last Measured Discharge and Learning count */
static void bq27541_battery_read_lmd_cyc(int *lmd, int *cycl, int *cyct)
{
	unsigned char hi, lo;
	int err1 = 0, err2 = 0;
	int error = 0;

	err1 = bq27541_i2c_read(BQ27541_CYCL_L, &lo);
	err2 = bq27541_i2c_read(BQ27541_CYCL_H, &hi);

	error = err1 | err2;

	if (!error)
		*cycl = (hi << 8) | lo;
}

/* Read Fall Available Capacity */
static int bq27541_battery_read_fac(int *fac)
{
	unsigned char hi, lo;
	int err1 = 0, err2 = 0;
	int error = 0;

	/* Clear out hi, lo for next reading */
	hi = lo = 0;

	err1 = bq27541_i2c_read(BQ27541_FAC_L, &lo);
	err2 = bq27541_i2c_read(BQ27541_FAC_H, &hi);

	error = err1 | err2;

	if (!error)
		*fac = (hi << 8) | lo;

	return error;
}

static int bq27541_battery_read_temperature(int *temperature)
{
	unsigned char temp_hi, temp_lo;
	int err1 = 0, err2 = 0;
	int celsius, fahrenheit;
	int error = 0;
	
	err1 = bq27541_i2c_read(BQ27541_TEMP_LOW, &temp_lo);
	err2 = bq27541_i2c_read(BQ27541_TEMP_HI, &temp_hi);
	
	error = err1 | err2;

	if (!error) {
		celsius = ((((temp_hi << 8) | temp_lo) + 2) / 4) - 273;
		fahrenheit = ((celsius * 9) / 5) + 32;
		*temperature = fahrenheit;
	}

	return error;
}

static int bq27541_battery_read_id(int *id)
{
	int error = 0;
	unsigned char value = 0xFF;

	error = bq27541_i2c_read(BQ27541_BATTERY_ID, &value);
	
	if (!error) {
		*id = value;
	}

	return error;
}

static void battery_handle_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(battery_work, battery_handle_work);

/* Main battery timer task */
static void battery_handle_work(struct work_struct *work)
{
	int err = 0;
	int batt_err_flag = 0;

	/* Is the battery driver stopped? */
	if (battery_driver_stopped)
		return;

	err = bq27541_battery_read_id(&bq27541_battery_id);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	err = bq27541_battery_read_temperature(&bq27541_temperature);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Check for the temperature range violation
	 */
	if ( (bq27541_temperature <= BQ27541_TEMP_LOW_THRESHOLD) ||
		(bq27541_temperature >= BQ27541_TEMP_HI_THRESHOLD) ) {
			temp_error_counter++;
			bq27541_reduce_charging = 0;
	}
	else {
		if (bq27541_temperature < BQ27541_TEMP_MID_THRESHOLD)
			bq27541_reduce_charging = 1;
		else
			bq27541_reduce_charging = 0;

		temp_error_counter = 0;
		bq27541_battery_error_flags &= ~TEMP_RANGE_ERROR;
	}

	if (temp_error_counter > BQ27541_ERROR_THRESHOLD) {
		bq27541_battery_error_flags |= TEMP_RANGE_ERROR;
		printk(KERN_ERR "battery driver temp - %d\n", bq27541_temperature);
		temp_error_counter = 0;
	}

	err = bq27541_battery_read_voltage(&bq27541_voltage);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Check for the battery voltage range violation
	 */
	if ( (bq27541_voltage <= BQ27541_VOLT_LOW_THRESHOLD) ||
		(bq27541_voltage >= BQ27541_VOLT_HI_THRESHOLD) ) {
			volt_error_counter++;
	}
	else {
		volt_error_counter = 0;
		bq27541_battery_error_flags &= ~VOLTAGE_ERROR;
	}

	if (volt_error_counter > BQ27541_ERROR_THRESHOLD) {
		printk(KERN_ERR "battery driver voltage - %dmV\n", bq27541_voltage);
		bq27541_battery_error_flags |= VOLTAGE_ERROR;
		volt_error_counter = 0;
	}

	/*
	 * Check for the battery current
	 */
	err = bq27541_battery_read_current(&bq27541_battery_current);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;	
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Read the gasguage capacity
	 */
	err = bq27541_battery_read_capacity(&bq27541_battery_capacity);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Read the battery mAH
	 */
	err = bq27541_battery_read_mAH(&bq27541_battery_mAH);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

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
	}
	else
		i2c_error_counter = 0;

	/* Full Available Capacity */
	err = bq27541_battery_read_fac(&bq27541_battery_fac);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

out:
	if (batt_err_flag) {
		i2c_error_counter++;
		if (i2c_error_counter == BQ27541_BATTERY_RETRY_THRESHOLD) {
			bq27541_battery_error_flags |= GENERAL_ERROR;
			printk(KERN_ERR "bq27541 battery: i2c read error, retry exceeded\n");
			i2c_error_counter = 0;
		}
	}
	else {
		bq27541_battery_error_flags &= ~GENERAL_ERROR;
		i2c_error_counter = 0;
	}

	pr_debug("temp: %d, volt: %d, current: %d, capacity: %d%%, mAH: %d\n",
		bq27541_temperature, bq27541_voltage, bq27541_battery_current,
		bq27541_battery_capacity, bq27541_battery_mAH);
	
	if (batt_err_flag & GENERAL_ERROR)
		schedule_delayed_work(&battery_work, msecs_to_jiffies(BQ27541_BATTERY_INTERVAL_ERROR));
	else
		schedule_delayed_work(&battery_work, msecs_to_jiffies(BQ27541_BATTERY_INTERVAL));
	return;
}

static const struct i2c_device_id bq27541_id[] =  {
        { "bq27541_Battery", 0 },
        { },
};
MODULE_DEVICE_TABLE(i2c, bq27541_id);

static int bq27541_get_property(struct power_supply *ps,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq27541_battery_present;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = bq27541_voltage;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = bq27541_battery_current;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = bq27541_battery_capacity;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = bq27541_temperature;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
		val->intval = bq27541_battery_tte;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = bq27541_battery_fac;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = bq27541_battery_mAH;
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
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static int bq27541_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct bq27541_info *info;
	int ret = 0;

        info = kzalloc(sizeof(*info), GFP_KERNEL);
        if (!info) {
                return -ENOMEM;
        }

        client->addr = BQ27541_I2C_ADDRESS;
        i2c_set_clientdata(client, info);

        info->client = client;
	info->battery.name = "bq27541_Battery";
	info->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	info->battery.get_property = bq27541_get_property;
	info->battery.properties = bq27541_battery_props;
	info->battery.num_properties = ARRAY_SIZE(bq27541_battery_props);

	ret = power_supply_register(&client->dev, &info->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		i2c_set_clientdata(client, NULL);
		kfree(info);
		return ret;
	}
	
        bq27541_battery_i2c_client = info->client;
        bq27541_battery_i2c_client->addr = BQ27541_I2C_ADDRESS;

	if (bq27541_battery_read_id(&bq27541_battery_id) < 0)
                return -ENODEV;

        if (bq27541_battery_sysdev_ctrl_init() < 0)
                printk(KERN_ERR "bq27541 battery: could not create sysfs entries\n");

	schedule_delayed_work(&battery_work, msecs_to_jiffies(BQ27541_BATTERY_INTERVAL_EARLY));

	bq27541_battery_present = 1;

        twl6030_interrupt_unmask(0x4,
                        REG_INT_MSK_STS_A);


        return 0;
}

static int bq27541_remove(struct i2c_client *client)
{
        struct bq27541_info *info = i2c_get_clientdata(client);

        if (bq27541_battery_present) {
                battery_driver_stopped = 1;
		cancel_rearming_delayed_work(&battery_work);
                bq27541_battery_sysdev_ctrl_exit();
                twl6030_interrupt_mask(0x4,
                        REG_INT_MSK_STS_A);
        }

        i2c_set_clientdata(client, info);

        return 0;
}

static int bq27541_battery_suspend(struct i2c_client *client, pm_message_t state)
{
	if (bq27541_battery_present) {
		battery_driver_stopped = 1;
		cancel_rearming_delayed_work(&battery_work);
		i2c_error_counter = 0;
	}

	return 0;
}
EXPORT_SYMBOL(bq27541_battery_suspend);

static int bq27541_battery_resume(struct i2c_client *client)
{
	battery_driver_stopped = 0;
	schedule_delayed_work(&battery_work, msecs_to_jiffies(BQ27541_BATTERY_INTERVAL));

	return 0;
}

static unsigned short normal_i2c[] = { BQ27541_I2C_ADDRESS, I2C_CLIENT_END };

static struct i2c_driver bq27541_i2c_driver = {
	.driver = {
			.name = DRIVER_NAME,
		},
	.probe = bq27541_probe,
	.remove = bq27541_remove,
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
	if (bq27541_battery_present) {
		battery_driver_stopped = 1;
		cancel_rearming_delayed_work(&battery_work);
		bq27541_battery_sysdev_ctrl_exit();
	}
	i2c_del_driver(&bq27541_i2c_driver);
}

module_init(bq27541_battery_init);
module_exit(bq27541_battery_exit);

MODULE_DESCRIPTION("BQ27541 Battery Driver");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
