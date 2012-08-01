/*
 * INVENSENSE MPU6050 Gyroscope driver
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Kishore Kadiyala <kishore.kadiyala@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/device.h>
#include "mpu6050x.h"
#include <linux/input/mpu6050.h>
#include <linux/i2c.h>

/*MPU6050 Gyro full scale range (degrees/sec) */
static int mpu6050_gyro_fsr_table[4] = {
	250,
	500,
	1000,
	2000,
};

/**
 * mpu6050_gyro_set_standby - Put's the gyro axis in standby or not
 * @mpu6050_gyro_data: gyroscope driver data
 * @enable: to put into standby mode or not
 */
static void mpu6050_gyro_set_standby(struct mpu6050_gyro_data *data,
							uint8_t enable)
{
	unsigned char val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_2,
							1, &val, "Gyro STBY");
	if (enable)
		val |= (MPU6050_STBY_XG |  MPU6050_STBY_YG |  MPU6050_STBY_ZG);
	else
		val &= ~(MPU6050_STBY_XG |  MPU6050_STBY_YG |  MPU6050_STBY_ZG);

	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
				MPU6050_REG_PWR_MGMT_2, val, "Gyro STBY");
}

/**
 * mpu6050_gyro_reset - Reset's the signal path of gyroscope
 * @mpu6050_gyro_data: gyroscope driver data
 */
static int mpu6050_gyro_reset(struct mpu6050_gyro_data *data)
{
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_SIGNAL_PATH_RESET, MPU6050_GYRO_SP_RESET,
							"Gyro SP-reset");
	mpu6050_gyro_set_standby(data, 0);
	return 0;
}

/**
 * mpu6050_gyro_read_xyz - read gyros's x-y-z axis data values
 * @mpu6050_gyro_data: gyroscope driver data
 */
static void mpu6050_gyro_read_xyz(struct mpu6050_gyro_data *data)
{
	int range;
	int16_t datax, datay, dataz;
	int16_t buffer[3];
	uint8_t val = 0;
	uint8_t fsr;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_INT_STATUS,
					1, &val, "interrupt status");
	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_GYRO_XOUT_H,
					6, (uint8_t *)buffer, "gyro-x-y-z");

	datax = be16_to_cpu(buffer[0]);
	datay = be16_to_cpu(buffer[1]);
	dataz = be16_to_cpu(buffer[2]);

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_GYRO_CONFIG, 1, &fsr, "Gyro full scale range");

	range = mpu6050_gyro_fsr_table[(fsr >> 3)];
	datax = (datax * range) / MPU6050_ABS_READING;
	datay = (datay * range) / MPU6050_ABS_READING;
	dataz = (dataz * range) / MPU6050_ABS_READING;

	input_report_rel(data->input_dev, REL_RX, datax);
	input_report_rel(data->input_dev, REL_RY, datay);
	input_report_rel(data->input_dev, REL_RZ, dataz);
	input_sync(data->input_dev);

}

static void mpu6050_gyro_input_work(struct work_struct *work)
{
	struct mpu6050_gyro_data *gyro =
				container_of((struct delayed_work *)work,
				struct mpu6050_gyro_data, input_work);

	mpu6050_gyro_read_xyz(gyro);
	schedule_delayed_work(&gyro->input_work,
			msecs_to_jiffies(gyro->req_poll_rate));
}

/**
 * mpu6050_gyro_suspend - driver suspend call
 * @mpu6050_gyro_data: gyroscope driver data
 */
void mpu6050_gyro_suspend(struct mpu6050_gyro_data *data)
{
	if (data->enabled)
		cancel_delayed_work_sync(&data->input_work);

	mpu6050_gyro_set_standby(data, 1);
}
EXPORT_SYMBOL(mpu6050_gyro_suspend);

/**
 * mpu6050_gyro_resume - driver resume call
 * @mpu6050_gyro_data: gyroscope driver data
 */
void mpu6050_gyro_resume(struct mpu6050_gyro_data *data)
{
	if (data->enabled) {
		mpu6050_gyro_set_standby(data, 0);
		schedule_delayed_work(&data->input_work, 0);
	}
}
EXPORT_SYMBOL(mpu6050_gyro_resume);


/**
 * mpu6050_gyro_show_attr_enable - sys entry to show gyro enable state.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds enable state value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_gyro_show_attr_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_gyro_data *data = mpu_data->gyro_data;

	return sprintf(buf, "%d\n", data->enabled);
}

/**
 * mpu6050_gyro_store_attr_enable - sys entry to set gyro enable state.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds enable state value.
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_gyro_store_attr_enable(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_gyro_data *data = mpu_data->gyro_data;
	unsigned long val;
	int error;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return error;

	if (val < 0 || val > 1)
		return -EINVAL;

	mutex_lock(&data->mutex);
	if (val) {
		mpu6050_gyro_set_standby(data, 0);
		data->enabled = 1;
		schedule_delayed_work(&data->input_work, 0);
	} else {
		mpu6050_gyro_set_standby(data, 1);
		data->enabled = 0;
		cancel_delayed_work_sync(&data->input_work);
	}
	mutex_unlock(&data->mutex);
	return count;
}

static DEVICE_ATTR(gyro_enable, S_IWUSR | S_IRUGO,
				mpu6050_gyro_show_attr_enable,
				mpu6050_gyro_store_attr_enable);

static struct attribute *mpu6050_gyro_attrs[] = {
	&dev_attr_gyro_enable.attr,
	NULL,
};

static struct attribute_group mpu6050_gyro_attr_group = {
	.attrs = mpu6050_gyro_attrs,
};

struct mpu6050_gyro_data *mpu6050_gyro_init(const struct mpu6050_data *mpu_data)
{
	struct mpu6050_gyro_platform_data *pdata;
	struct mpu6050_gyro_data *gyro_data;
	struct input_dev *input_dev;
	int error;

	/* Driver data memory allocation */
	gyro_data = kzalloc(sizeof(*gyro_data), GFP_KERNEL);
	if (!gyro_data) {
		error = -ENOMEM;
		goto err_out;
	}

	/* Input device allocation */
	input_dev = input_allocate_device();
	if (!input_dev) {
		error = -ENOMEM;
		goto err_free_mem;
	}

	/* Obtain driver platform data info */
	pdata = &mpu_data->pdata->mpu6050_gyro;

	/* Initialize device driver data */
	gyro_data->req_poll_rate = pdata->def_poll_rate;
	gyro_data->dev = mpu_data->dev;
	gyro_data->input_dev = input_dev;
	gyro_data->bus_ops = mpu_data->bus_ops;
	gyro_data->enabled = 0; /* disabled by default */
	mutex_init(&gyro_data->mutex);

	/* Configure the init values from platform data */
	MPU6050_WRITE(gyro_data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_GYRO_CONFIG, (pdata->fsr << 3), "Init fsr");

	/* Initialize the Input Subsystem configuration */
	input_dev->name = "mpu6050-gyroscope";
	input_dev->id.bustype = mpu_data->bus_ops->bustype;

	input_set_capability(gyro_data->input_dev, EV_REL, REL_RX);
	input_set_capability(gyro_data->input_dev, EV_REL, REL_RY);
	input_set_capability(gyro_data->input_dev, EV_REL, REL_RZ);

	input_set_drvdata(input_dev, gyro_data);

	/*  Reset Gyroscope signal path */
	mpu6050_gyro_reset(gyro_data);

	/* Register with the Input Subsystem */
	error = input_register_device(gyro_data->input_dev);
	if (error) {
		dev_err(gyro_data->dev, "Unable to register input device\n");
		goto err_free_input;
	}

	/* Create sysfs driver sysfs world */
	error = sysfs_create_group(&mpu_data->client->dev.kobj,
					&mpu6050_gyro_attr_group);
	if (error) {
		dev_err(&mpu_data->client->dev,
			"failed to create sysfs entries\n");
		goto err_sysfs_failed;
	}

	INIT_DELAYED_WORK(&gyro_data->input_work, mpu6050_gyro_input_work);

	/* Set initial gyroscope state */
	if (gyro_data->enabled)
		mpu6050_gyro_set_standby(gyro_data, 0);
	else
		mpu6050_gyro_set_standby(gyro_data, 1);

	return gyro_data;

err_sysfs_failed:
	input_unregister_device(gyro_data->input_dev);
err_free_input:
	mutex_destroy(&gyro_data->mutex);
	input_free_device(input_dev);
err_free_mem:
	kfree(gyro_data);
err_out:
	return ERR_PTR(error);
}
EXPORT_SYMBOL(mpu6050_gyro_init);

void mpu6050_gyro_exit(struct mpu6050_gyro_data *data)
{
	struct i2c_client *client = to_i2c_client(data->dev);
	cancel_delayed_work_sync(&data->input_work);
	sysfs_remove_group(&client->dev.kobj, &mpu6050_gyro_attr_group);
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	mutex_destroy(&data->mutex);
	kfree(data);
}
EXPORT_SYMBOL(mpu6050_gyro_exit);

MODULE_AUTHOR("Kishore Kadiyala <kishore.kadiyala@ti.com");
MODULE_DESCRIPTION("MPU6050 I2c Driver");
MODULE_LICENSE("GPL");
