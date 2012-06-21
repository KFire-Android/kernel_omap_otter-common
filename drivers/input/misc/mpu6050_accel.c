/*
 * INVENSENSE MPU6050 Accelerometer driver
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Kishore Kadiyala <kishore.kadiyala@ti.com>
 * Contributor: Leed Aguilar <leed.aguilar@ti.com>
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
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/input/mpu6050.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include "mpu6050x.h"


/*MPU6050 full scale range types*/
static int mpu6050_grange_table[4] = {
	2000,
	4000,
	8000,
	16000,
};

/**
 * mpu6050_accel_set_standby - Put's the accel axis in standby or not
 * @mpu6050_accel_data: accelerometer driver data
 * @enable: to put into standby mode or not
 */
static void mpu6050_accel_set_standby(struct mpu6050_accel_data *data,
							uint8_t enable)
{
	uint8_t val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_PWR_MGMT_2,
							1, &val, "Accel STBY");
	if (enable)
		val |= (MPU6050_STBY_XA |  MPU6050_STBY_YA |  MPU6050_STBY_ZA);
	else
		val &= ~(MPU6050_STBY_XA |  MPU6050_STBY_YA |  MPU6050_STBY_ZA);

	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
			MPU6050_REG_PWR_MGMT_2, val, "Accel STBY");
}

/**
 * mpu6050_accel_reset - Reset's the signal path of acceleromter
 * @mpu6050_accel_data: accelerometer driver data
 */
static void mpu6050_accel_reset(struct mpu6050_accel_data *data)
{
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_SIGNAL_PATH_RESET, MPU6050_ACCEL_SP_RESET,
							"Accel SP-reset");
	mpu6050_accel_set_standby(data, 0);
}

/**
 * mpu6050_data_decode_mg - Data in 2's complement, convert to mg
 * @mpu6050_accel_data: accelerometer driver data
 * @datax: X axis data value
 * @datay: Y axis data value
 * @dataz: Z axis data value
 */
static void mpu6050_data_decode_mg(struct mpu6050_accel_data *data,
				int16_t *datax, int16_t *datay, int16_t *dataz)
{
	uint8_t fsr;
	int range;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_CONFIG, 1, &fsr, "full scale range");
	range = mpu6050_grange_table[(fsr >> 3)];
	/* Data in 2's complement, convert to mg */
	*datax = (*datax * range) / MPU6050_ABS_READING;
	*datay = (*datay * range) / MPU6050_ABS_READING;
	*dataz = (*dataz * range) / MPU6050_ABS_READING;
}

/**
 * mpu6050_accel_thread_irq - mpu6050 interrupt handler
 * @irq: device interrupt number
 * @dev_id: pointer to driver device data
 */
static irqreturn_t mpu6050_accel_thread_irq(int irq, void *dev_id)
{
	struct mpu6050_accel_data *data = dev_id;
	int16_t datax, datay, dataz;
	uint16_t buffer[3];
	uint8_t val = 0;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_INT_STATUS,
						1, &val, "interrupt status");
	dev_dbg(data->dev, "MPU6050 Interrupt status Reg=%x\n", val);
	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_XOUT_H, 6, (uint8_t *)buffer, "Accel-x-y-z");

	datax = be16_to_cpu(buffer[0]);
	datay = be16_to_cpu(buffer[1]);
	dataz = be16_to_cpu(buffer[2]);

	mpu6050_data_decode_mg(data, &datax, &datay, &dataz);

	input_report_abs(data->input_dev, ABS_X, datax);
	input_report_abs(data->input_dev, ABS_Y, datay);
	input_report_abs(data->input_dev, ABS_Z, dataz);
	input_sync(data->input_dev);

	return IRQ_HANDLED;
}

/**
 * mpu6050_accel_suspend - driver suspend call
 * @mpu6050_accel_data: accelerometer driver data
 */
void mpu6050_accel_suspend(struct mpu6050_accel_data *data)
{
	if (data->enabled)
		mpu6050_accel_set_standby(data, 1);
}
EXPORT_SYMBOL(mpu6050_accel_suspend);

/**
 * mpu6050_accel_suspend - driver resume call
 * @mpu6050_accel_data: accelerometer driver data
 */
void mpu6050_accel_resume(struct mpu6050_accel_data *data)
{
	if (data->enabled)
		mpu6050_accel_set_standby(data, 0);
}
EXPORT_SYMBOL(mpu6050_accel_resume);

/**
 * mpu6050_accel_show_attr_modedur - sys entry to show threshold duration.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duration value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_modedur(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t modedur;
	uint8_t reg = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_DUR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_DUR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_DUR;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, reg, 1,
					&modedur, "Attr mode Duration");
	return sprintf(buf, "%x\n", modedur);
}

/**
 * mpu6050_accel_store_attr_modedur - sys entry to set threshold duration.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_modedur(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;
	uint8_t reg = 0;

	error = kstrtoul(buf, 16, &val);
	if (error)
		return error;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_DUR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_DUR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_DUR;

	mutex_lock(&data->mutex);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, reg, val,
						"Attr mode Duration");
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_modethr - sys entry to show threshold value.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_modethr(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t modethr;
	uint8_t reg = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_THR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_THR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_THR;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR, reg, 1,
					&modethr, "Attr mode threshold");
	return sprintf(buf, "%x\n", modethr);
}

/**
 * mpu6050_accel_store_attr_modethr - sys entry to set threshold value.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_modethr(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;
	uint8_t reg = 0;

	error = kstrtoul(buf, 16, &val);
	if (error)
		return error;

	if (data->mode == FREE_FALL_MODE)
		reg = MPU6050_REG_ACCEL_FF_THR;
	else if (data->mode == MOTION_DET_MODE)
		reg = MPU6050_REG_ACCEL_MOT_THR;
	else
		reg = MPU6050_REG_ACCEL_ZRMOT_THR;

	mutex_lock(&data->mutex);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, reg, val,
						"Attr mode threshold");
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_confmode - sys entry to show accel operational mode.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duration value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_confmode(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	return sprintf(buf, "%d\n", data->mode);
}

/**
 * mpu6050_accel_store_attr_confmode - sys entry to set accel operational mode
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_confmode(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error = -1;

	error = kstrtoul(buf, 10, &val);
	if (error)
		return error;

	if (val < FREE_FALL_MODE || val > ZERO_MOT_DET_MODE)
		return -EINVAL;

	mutex_lock(&data->mutex);
	data->mode = (enum accel_op_mode) val;
	if (data->mode == FREE_FALL_MODE)
		val = 1 << 7;
	else if (data->mode == MOTION_DET_MODE)
		val = 1 << 6;
	else
		val = 1 << 5;
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_INT_ENABLE, val,
								"Attr conf mode");
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_enable - sys entry to show accel enable state.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds enable state value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_enable(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	return sprintf(buf, "%d\n", data->enabled);
}

/**
 * mpu6050_accel_store_attr_enable - sys entry to set accel enable state.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds enable state value.
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_enable(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return error;

	if (val < 0 || val > 1)
		return -EINVAL;

	mutex_lock(&data->mutex);
	if (data->enabled != val) {

		if (val)
			mpu6050_accel_set_standby(data, 0);
		else
			mpu6050_accel_set_standby(data, 1);

		data->enabled = val;
	}
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_store_attr_ast - sys entry to set the axis for self test
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_ast(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	uint8_t reg;
	int error = -1;

	error = kstrtoul(buf, 10, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	if (val == 1)
		reg = (0x1 << 7);
	else if (val == 2)
		reg = (0x1 << 6);
	else if (val == 3)
		reg = (0x1 << 5);
	else
		return error;

	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG,
							reg, "Accel Self Test");
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_hpf - sys entry to show DHPF configuration.
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_hpf(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t hpf;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
			MPU6050_REG_ACCEL_CONFIG, 1, &hpf, "Attr show fsr");
	return sprintf(buf, "%d\n", hpf);
}

/**
 * mpu6050_accel_store_attr_hpf- sys entry to set the DHPF configuration
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_hpf(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;

	error = kstrtoul(buf, 16, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG,
							val, "Attr store fsr");
	mutex_unlock(&data->mutex);
	return count;
}

/**
 * mpu6050_accel_show_attr_fsr- sys entry to show full scale range
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds threshold duartion value.
 *
 * Returns '0' on success.
 */
static ssize_t mpu6050_accel_show_attr_fsr(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t fsr;
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;

	MPU6050_READ(data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_ACCEL_CONFIG, 1, &fsr, "Attr show fsr");
	return sprintf(buf, "%d\n", (fsr >> 3));
}

/**
 * mpu6050_accel_store_attr_fsr - sys entry to set the full scale range
 * @dev: device entry
 * @attr: device attribute entry
 * @buf: pointer to the buffer which holds input threshold duartion value.
 * @count: reference count
 *
 * Returns 'count' on success.
 */
static ssize_t mpu6050_accel_store_attr_fsr(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu6050_data *mpu_data = platform_get_drvdata(pdev);
	struct mpu6050_accel_data *data = mpu_data->accel_data;
	unsigned long val;
	int error;

	error = kstrtoul(buf, 16, &val);
	if (error)
		return error;
	mutex_lock(&data->mutex);
	MPU6050_WRITE(data, MPU6050_CHIP_I2C_ADDR, MPU6050_REG_ACCEL_CONFIG,
						(val << 3), "Attr store fsr");
	mutex_unlock(&data->mutex);
	return count;
}

static DEVICE_ATTR(accel_fsr, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_fsr,
		mpu6050_accel_store_attr_fsr);

static DEVICE_ATTR(accel_hpf, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_hpf,
		mpu6050_accel_store_attr_hpf);

static DEVICE_ATTR(accel_ast, S_IWUSR | S_IRUGO, NULL,
		mpu6050_accel_store_attr_ast);

static DEVICE_ATTR(accel_confmode, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_confmode,
		mpu6050_accel_store_attr_confmode);

static DEVICE_ATTR(accel_modedur, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_modedur,
		mpu6050_accel_store_attr_modedur);

static DEVICE_ATTR(accel_modethr, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_modethr,
		mpu6050_accel_store_attr_modethr);
static DEVICE_ATTR(accel_enable, S_IWUSR | S_IRUGO,
		mpu6050_accel_show_attr_enable,
		mpu6050_accel_store_attr_enable);


static struct attribute *mpu6050_accel_attrs[] = {
	&dev_attr_accel_fsr.attr,
	&dev_attr_accel_hpf.attr,
	&dev_attr_accel_ast.attr,
	&dev_attr_accel_confmode.attr,
	&dev_attr_accel_modedur.attr,
	&dev_attr_accel_modethr.attr,
	&dev_attr_accel_enable.attr,
	NULL,
};

static struct attribute_group mpu6050_accel_attr_group = {
	.attrs = mpu6050_accel_attrs,
};

struct mpu6050_accel_data *mpu6050_accel_init(
			const struct mpu6050_data *mpu_data)
{
	const struct mpu6050_accel_platform_data *pdata;
	struct mpu6050_accel_data *accel_data;
	struct input_dev *input_dev;
	int error;
	uint8_t reg_thr;
	uint8_t reg_dur;
	uint8_t intr_mask;
	uint8_t val;

	/* Verify for IRQ line */
	if (!mpu_data->irq) {
		pr_err("%s: Accelerometer Irq not assigned\n", __func__);
		error = -EINVAL;
		goto err_out;
	}

	/* Driver data memory allocation */
	accel_data = kzalloc(sizeof(*accel_data), GFP_KERNEL);
	if (!accel_data) {
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
	pdata = &mpu_data->pdata->mpu6050_accel;

	/* Initialize device driver data */
	accel_data->dev = mpu_data->dev;
	accel_data->input_dev = input_dev;
	accel_data->bus_ops = mpu_data->bus_ops;
	accel_data->gpio = mpu_data->irq;
	accel_data->irq = gpio_to_irq(accel_data->gpio);
	accel_data->enabled = 0; /* disabled by default */
	accel_data->mode = pdata->ctrl_mode;
	mutex_init(&accel_data->mutex);

	/* Configure the init values from platform data */
	error = MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
			MPU6050_REG_ACCEL_CONFIG,
			(pdata->fsr << 3), "Init fsr");
	if (error) {
		dev_err(accel_data->dev, "fail to configure FSR\n");
		goto err_free_input;
	}

	error = MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
			MPU6050_REG_ACCEL_CONFIG,
			pdata->hpf, "Init hpf");
	if (error) {
		dev_err(accel_data->dev, "fail to configure HPF\n");
		goto err_free_input;
	}

	if (pdata->ctrl_mode == FREE_FALL_MODE) {
		reg_thr = MPU6050_REG_ACCEL_FF_THR;
		reg_dur = MPU6050_REG_ACCEL_FF_DUR;
		intr_mask = MPU6050_FF_INT;
	} else if (pdata->ctrl_mode == MOTION_DET_MODE) {
		reg_thr = MPU6050_REG_ACCEL_MOT_THR;
		reg_dur = MPU6050_REG_ACCEL_MOT_DUR;
		intr_mask = MPU6050_MOT_INT;
	} else {
		reg_thr = MPU6050_REG_ACCEL_MOT_THR;
		reg_dur = MPU6050_REG_ACCEL_MOT_DUR;
		intr_mask = MPU6050_ZMOT_INT;
	}

	error = MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
			reg_thr, pdata->mode_thr_val, "Init thr");
	if (error) {
		dev_err(accel_data->dev, "fail to configure threshold value\n");
		goto err_free_input;
	}

	error = MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
			reg_dur, pdata->mode_thr_dur, "Init dur");
	if (error) {
		dev_err(accel_data->dev, "fail to configure thr duration\n");
		goto err_free_input;
	}

	/* Initialize the Input Subsystem configuration */
	input_dev->name = "mpu6050-accelerometer";
	input_dev->id.bustype = mpu_data->bus_ops->bustype;

	 __set_bit(EV_ABS, input_dev->evbit);

	input_set_abs_params(input_dev, ABS_X,
			-mpu6050_grange_table[pdata->fsr],
			mpu6050_grange_table[pdata->fsr], pdata->x_axis, 0);
	input_set_abs_params(input_dev, ABS_Y,
			-mpu6050_grange_table[pdata->fsr],
			mpu6050_grange_table[pdata->fsr], pdata->y_axis, 0);
	input_set_abs_params(input_dev, ABS_Z,
			-mpu6050_grange_table[pdata->fsr],
			mpu6050_grange_table[pdata->fsr], pdata->z_axis, 0);

	input_set_drvdata(input_dev, accel_data);

	/* Reset Accelerometer signal path */
	mpu6050_accel_reset(accel_data);

	/* Initialize the IRQ support */
	error = gpio_request_one(accel_data->gpio, GPIOF_IN, "accel");
	if (error) {
		dev_err(accel_data->dev, "sensor: gpio request failure\n");
		goto err_free_input;
	}
	error = request_threaded_irq(accel_data->irq, NULL,
					mpu6050_accel_thread_irq,
					pdata->irqflags | IRQF_ONESHOT,
					"mpu6050_accel_irq", accel_data);
	if (error) {
		dev_err(accel_data->dev, "request_threaded_irq failed\n");
		goto err_free_gpio;
	}

	/* Register with the Input Subsystem */
	error = input_register_device(accel_data->input_dev);
	if (error) {
		dev_err(accel_data->dev, "Unable to register input device\n");
		goto err_free_irq;
	}

	/* Create sysfs world */
	error = sysfs_create_group(&mpu_data->client->dev.kobj,
					&mpu6050_accel_attr_group);
	if (error) {
		dev_err(&mpu_data->client->dev,
			"failed to create sysfs entries\n");
		goto err_sysfs_failed;
	}

	/* Enable interrupt for requested mode */
	error = MPU6050_WRITE(accel_data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_INT_ENABLE, intr_mask, "Enable Intr");
	if (error) {
		dev_err(accel_data->dev, "fail to enable interrupt\n");
		goto err_reg_device;
	}
	/* Clear interrupts for requested mode */
	error = MPU6050_READ(accel_data, MPU6050_CHIP_I2C_ADDR,
		MPU6050_REG_INT_STATUS, 1, &val, "Clear Isr");
	if (error) {
		dev_err(accel_data->dev, "fail to clear ISR\n");
		goto err_reg_device;
	}

	/* Set initial Accelerometer state */
	if (accel_data->enabled)
		mpu6050_accel_set_standby(accel_data, 0);
	else
		mpu6050_accel_set_standby(accel_data, 1);

	return accel_data;

err_reg_device:
	sysfs_remove_group(&mpu_data->client->dev.kobj,
				&mpu6050_accel_attr_group);
err_sysfs_failed:
	input_unregister_device(accel_data->input_dev);
err_free_irq:
	free_irq(accel_data->irq, accel_data);
err_free_gpio:
	gpio_free(mpu_data->client->irq);
err_free_input:
	mutex_destroy(&accel_data->mutex);
	input_free_device(accel_data->input_dev);
err_free_mem:
	kfree(accel_data);
err_out:
	return ERR_PTR(error);
}
EXPORT_SYMBOL(mpu6050_accel_init);

void mpu6050_accel_exit(struct mpu6050_accel_data *data)
{
	struct i2c_client *client = to_i2c_client(data->dev);

	sysfs_remove_group(&client->dev.kobj, &mpu6050_accel_attr_group);
	input_unregister_device(data->input_dev);
	free_irq(data->irq, data);
	gpio_free(data->gpio);
	input_free_device(data->input_dev);
	mutex_destroy(&data->mutex);
	kfree(data);
}
EXPORT_SYMBOL(mpu6050_accel_exit);

MODULE_AUTHOR("Kishore Kadiyala <kishore.kadiyala@ti.com");
MODULE_DESCRIPTION("MPU6050 I2c Driver");
MODULE_LICENSE("GPL");
