/*
 * mpu3050.c
 * MPU-3050 Gyroscope driver
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Dan Murphy <Dmurphy@ti.com>
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
 *
 * Derived work from mpu3050_gyro.c from Jorge Bustamante <jbustamante@ti.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/i2c/mpu3050.h>
#include <linux/gpio.h>

#define MPU3050_DEBUG 1

#define DEVICE_NAME "mpu3050_gyro"
#define DRIVER_NAME "mpu3050_gyro"

#define MPU3050_WHO_AM_I		0x00
#define MPU3050_PRODUCT_ID		0x01
#define MPU3050_X_OFFS_USRH		0x0C
#define MPU3050_X_OFFS_USRL		0x0D
#define MPU3050_Y_OFFS_USRH		0x0E
#define MPU3050_Y_OFFS_USRL		0x0F
#define MPU3050_Z_OFFS_USRH		0x10
#define MPU3050_Z_OFFS_USRL		0x11
#define MPU3050_IME_SLV_ADDR		0x14
#define MPU3050_SMPLRT_DIV		0x15
#define MPU3050_DLPF_FS_SYNC		0x16
#define MPU3050_INT_CFG			0x17
#define MPU3050_ACCEL_BUS_LVL		0x18
#define MPU3050_INT_STATUS		0x1A
#define MPU3050_TEMP_OUT_H		0x1B
#define MPU3050_TEMP_OUT_L		0x1C
#define MPU3050_GYRO_XOUT_H		0x1D
#define MPU3050_GYRO_XOUT_L		0x1E
#define MPU3050_GYRO_YOUT_H		0x1F
#define MPU3050_GYRO_YOUT_L		0x20
#define MPU3050_GYRO_ZOUT_H		0x21
#define MPU3050_GYRO_ZOUT_L		0x22
#define MPU3050_FIFO_COUNTH		0x3A
#define MPU3050_FIFO_COUNTL		0x3B
#define MPU3050_FIFO_R			0x3C
#define MPU3050_USER_CTRL		0x3D
#define MPU3050_PWR_MGMT		0x3E

#define SLEEP				0x00
#define WAKEUP				0x01

struct mpu3050_gyro_data {
	struct mpu3050gyro_platform_data *pdata;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *wq;
	struct delayed_work d_work;
	struct mutex mutex;

	int enable;
	int def_poll_rate;
	int wakeup_flag;
};

static uint32_t gyro_debug;
module_param_named(mpu3050_debug, gyro_debug, uint, 0664);

#ifdef MPU3050_DEBUG
struct mpu3050_reg {
	const char *name;
	uint8_t reg;
	int writeable;
} mpu3050_regs[] = {
	{ "CHIP_ID",		MPU3050_WHO_AM_I, 0 },
	{ "PRODUCT_ID",		MPU3050_PRODUCT_ID, 0 },
	{ "X_OFFSET_HI",	MPU3050_X_OFFS_USRH, 1},
	{ "X_OFFSET_LOW",	MPU3050_X_OFFS_USRL, 1},
	{ "Y_OFFSET_HI",	MPU3050_Y_OFFS_USRH, 1},
	{ "Y_OFFSET_LOW",	MPU3050_Y_OFFS_USRL, 1},
	{ "Z_OFFSET_HI",	MPU3050_Z_OFFS_USRH, 1},
	{ "Z_OFFSET_LOW",	MPU3050_Z_OFFS_USRL, 1},
	{ "SLAVE_I2C_ADDR",	MPU3050_IME_SLV_ADDR, 1},
	{ "SAMPLE_DIVISOR",	MPU3050_SMPLRT_DIV, 1},
	{ "DLPF_FS_SYNC",	MPU3050_DLPF_FS_SYNC, 1},
	{ "INT_CFG",		MPU3050_INT_CFG, 1},
	{ "ACCEL_BUS_LVL",	MPU3050_ACCEL_BUS_LVL, 1},
	{ "INT_STATUS",		MPU3050_INT_STATUS, 0},
	{ "TEMP_HIGH",		MPU3050_INT_STATUS, 0},
	{ "TEMP_LOW",		MPU3050_INT_STATUS, 0},
	{ "XOUT_HIGH",		MPU3050_GYRO_XOUT_H, 0},
	{ "XOUT_LOW",		MPU3050_GYRO_XOUT_L, 0},
	{ "YOUT_HIGH",		MPU3050_GYRO_YOUT_H, 0},
	{ "YOUT_LOW",		MPU3050_GYRO_YOUT_L, 0},
	{ "ZOUT_HIGH",		MPU3050_GYRO_ZOUT_H, 0},
	{ "ZOUT_LOW",		MPU3050_GYRO_ZOUT_L, 0},
	{ "FIFO_CNT_H",		MPU3050_FIFO_COUNTH, 0},
	{ "FIFO_CNT_L",		MPU3050_FIFO_COUNTL, 0},
	{ "FIFO_R",		MPU3050_FIFO_R, 0},
	{ "USR_CTRL",		MPU3050_USER_CTRL, 0},
	{ "PWR_MGMT",		MPU3050_PWR_MGMT, 0},

};
#endif

static int mpu3050_write(struct mpu3050_gyro_data *data,
				u8 reg, u8 val)
{
	int ret = i2c_smbus_write_byte_data(data->client, reg, val);
	if (ret < 0)
		dev_err(&data->client->dev,
			"i2c_smbus_write_byte_data failed\n");
	return ret;
}

static int mpu3050_read_transfer(struct mpu3050_gyro_data *data,
		unsigned short data_addr, char *data_buf, int count)
{
	int ret;
	int counter = 5;
	char *data_buffer = data_buf;

	struct i2c_msg msgs[] = {
	{
		.addr = data->client->addr,
		.flags = data->client->flags & I2C_M_TEN,
		.len = 1,
		.buf = data_buffer,
		},
		{
		.addr = data->client->addr,
		.flags = (data->client->flags & I2C_M_TEN) | I2C_M_RD,
		.len = count,
		.buf = data_buffer,
		},
	};

	data_buffer[0] = data_addr;
	msgs->buf = data_buffer;

	do {
		ret = i2c_transfer(data->client->adapter, msgs, 2);
		if (ret != 2) {
			dev_err(&data->client->dev,
				"i2c_transfer failed\n");
			counter--;
			msleep(1);
		} else {
			return 0;
		}
	} while (counter >= 0);

	return -1;
}

static void mpu3050_device_sleep_wakeup(struct mpu3050_gyro_data *data,
							 int do_wakeup)
{
	int ret;
	uint8_t reg_val;

	mpu3050_read_transfer(data, MPU3050_PWR_MGMT, &reg_val, 1);

	if (do_wakeup) {
		reg_val &= ~MPU3050_PWR_MGMT_SLEEP;
		ret = mpu3050_write(data, MPU3050_PWR_MGMT, reg_val);
		if (ret < 0)
			dev_err(&data->client->dev,
				"can't wakeup device\n");
		msleep(50);
	} else {
		reg_val |= MPU3050_PWR_MGMT_SLEEP;
		ret = mpu3050_write(data, MPU3050_PWR_MGMT, reg_val);
		if (ret < 0)
			dev_err(&data->client->dev,
				"sleep mode can't be set\n");
	}
}

static int mpu3050_data_ready(struct mpu3050_gyro_data *data)
{
	int ret;
	uint8_t data_val_h, data_val_l;
	short int x = 0;
	short int y = 0;
	short int z = 0;
	short int temp = 0;
	uint8_t data_buffer[8];

	ret = mpu3050_read_transfer(data, MPU3050_TEMP_OUT_H, data_buffer, 8);
	if (ret != 0) {
		dev_err(&data->client->dev,
			"mpu3050_read_data_ready failed\n");
		return -1;
	}
	data_val_h = data_buffer[0];
	data_val_l = data_buffer[1];

	temp = ((data_val_h << 8) | data_val_l);
	if (gyro_debug)
		pr_info("%s: temp low 0x%X temp high 0x%X\n",
		__func__, data_val_l, data_val_h);


	data_val_h = data_buffer[2];
	data_val_l = data_buffer[3];
	if (gyro_debug)
		pr_info("%s: X low 0x%X X high 0x%X\n",
		__func__, data_val_l, data_val_h);
	x = ((data_val_h << 8) | data_val_l);

	data_val_h = data_buffer[4];
	data_val_l = data_buffer[5];
	if (gyro_debug)
		pr_info("%s: Y low 0x%X Y high 0x%X\n",
		__func__, data_val_l, data_val_h);

	y = ((data_val_h << 8) | data_val_l);

	data_val_h = data_buffer[6];
	data_val_l = data_buffer[7];
	if (gyro_debug)
		pr_info("%s: Z low 0x%X Z high 0x%X\n",
		__func__, data_val_l, data_val_h);

	z = ((data_val_h << 8) | data_val_l);
	if (gyro_debug)
		pr_info("%s: X: 0x%X Y: 0x%X Z: 0x%X\n",
			__func__, x, y, z);
	input_report_rel(data->input_dev, REL_RX, x);
	input_report_rel(data->input_dev, REL_RY, y);
	input_report_rel(data->input_dev, REL_RZ, z);
	input_sync(data->input_dev);

	return 0;
}

static int mpu3050_device_autocalibrate(struct mpu3050_gyro_data *data)
{
	int ret;
	uint8_t data_val_h, data_val_l;
	short int data_val;
	uint8_t data_buffer[6];

	ret = mpu3050_read_transfer(data, MPU3050_GYRO_XOUT_H, data_buffer, 6);
	if (ret != 0) {
		dev_err(&data->client->dev,
			"mpu3050_device_autocalibrate failed\n");
		return -1;
	}

	/* read the values on a stable position (no movement) */
	data_val_h = data_buffer[0];
	data_val_l = data_buffer[1];
	/* get the 2's complement of data_val and
	leave the corresponding values on data_val_h and data_val_l */
	data_val = (short int) ((data_val_h << 8) | data_val_l);
	data_val = -data_val;
	data_val_h = (data_val >> 8);
	data_val_l = (data_val & 0x00FF);
	/* write the value to the corresponding offset register */
	mpu3050_write(data, MPU3050_X_OFFS_USRH, data_val_h);
	mpu3050_write(data, MPU3050_X_OFFS_USRL, data_val_l);

	/* read the values on a stable position (no movement) */
	data_val_h = data_buffer[2];
	data_val_l = data_buffer[3];
	/* get the 2's complement of data_val and leave the
	corresponding values on data_val_h and data_val_l */
	data_val = (short int) ((data_val_h << 8) | data_val_l);
	data_val = -data_val;
	data_val_h = (data_val >> 8);
	data_val_l = (data_val & 0x00FF);
	/* write the value to the corresponding offset register */
	mpu3050_write(data, MPU3050_Y_OFFS_USRH, data_val_h);
	mpu3050_write(data, MPU3050_Y_OFFS_USRL, data_val_l);

	/* read the values on a stable position (no movement) */
	data_val_h = data_buffer[4];
	data_val_l = data_buffer[5];
	/* get the 2's complement of data_val and leave the
	corresponding values on data_val_h and data_val_l */
	data_val = (short int) ((data_val_h << 8) | data_val_l);
	data_val = -data_val;
	data_val_h = (data_val >> 8);
	data_val_l = (data_val & 0x00FF);
	/* write the value to the corresponding offset register */
	mpu3050_write(data, MPU3050_Z_OFFS_USRH, data_val_h);
	mpu3050_write(data, MPU3050_Z_OFFS_USRL, data_val_l);
	return 0;
}

static irqreturn_t mpu3050_thread_irq(int irq, void *dev_data)
{
	uint8_t reg_val;
	struct mpu3050_gyro_data *data = (struct mpu3050_gyro_data *) dev_data;

	mpu3050_read_transfer(data, MPU3050_INT_STATUS, &reg_val, 1);

	if (reg_val & MPU3050_INT_STATUS_MPU_RDY) {
#ifdef MPU3050_DEBUG
		printk(KERN_ALERT "%s: interrupt: MPU ready.", __func__);
#endif
	} else if (reg_val & MPU3050_INT_STATUS_RAW_DATA_RDY) {
		mpu3050_data_ready(data);
	} else {
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int mpu3050_device_work(void *work)
{
	uint8_t reg_val;
	struct mpu3050_gyro_data *data =
		container_of((struct delayed_work *)work,
			      struct mpu3050_gyro_data, d_work);

	mpu3050_read_transfer(data, MPU3050_INT_STATUS, &reg_val, 1);
	if (reg_val & MPU3050_INT_STATUS_RAW_DATA_RDY) {
		mpu3050_data_ready(data);
		queue_delayed_work(data->wq, &data->d_work,
			msecs_to_jiffies(data->def_poll_rate));
	} else
		queue_delayed_work(data->wq, &data->d_work, 100);

	return 0;
}

static int mpu3050_device_hw_init(struct mpu3050_gyro_data *data)
{
	int ret = 0;
	uint8_t reg_val;

	mpu3050_write(data, MPU3050_PWR_MGMT,
		(MPU3050_PWR_MGMT_RESET | MPU3050_PWR_MGMT_CLK_STOP_RESET));
	mpu3050_write(data, MPU3050_PWR_MGMT, 0x00);
	mpu3050_write(data, MPU3050_WHO_AM_I, data->client->addr);
	mpu3050_write(data, MPU3050_IME_SLV_ADDR, data->pdata->slave_i2c_addr);
	mpu3050_write(data, MPU3050_SMPLRT_DIV, data->pdata->sample_rate_div);
	mpu3050_write(data, MPU3050_DLPF_FS_SYNC, data->pdata->dlpf_fs_sync);
	mpu3050_write(data, MPU3050_INT_CFG, data->pdata->interrupt_cfg);
	mpu3050_write(data, MPU3050_ACCEL_BUS_LVL, 0x00);

	msleep(50);

	/* auto calibrate (dont move the IC while this function is called) */
	mpu3050_device_autocalibrate(data);

	mpu3050_read_transfer(data, MPU3050_INT_STATUS, &reg_val, 1);

	return ret;
}

static ssize_t mpu3050_show_attr_enable(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", data->enable);
}

static ssize_t mpu3050_store_attr_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	if (data->enable == !!val) {
		mutex_unlock(&data->mutex);
		return count;
	}

	data->enable = !!val;
	mutex_unlock(&data->mutex);

	if (data->enable) {
		data->wakeup_flag = 0;
		mpu3050_device_sleep_wakeup(data, WAKEUP);
		if (data->client->irq)
			enable_irq(data->client->irq);
		else
			queue_delayed_work(data->wq, &data->d_work, 0);
	} else {
		if (data->client->irq)
			disable_irq_nosync(data->client->irq);
		else
			cancel_delayed_work_sync(&data->d_work);
		mpu3050_device_sleep_wakeup(data, SLEEP);
	}
	return count;
}

static ssize_t mpu3050_show_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", data->def_poll_rate);
}

static ssize_t mpu3050_store_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);
	unsigned long interval;
	int error;

	error = strict_strtoul(buf, 0, &interval);
	if (error)
		return error;

	if (interval <= 0 || interval > 200)
		return -EINVAL;

	if (data->client->irq)
		disable_irq_nosync(data->client->irq);
	else
		cancel_delayed_work_sync(&data->d_work);

	data->def_poll_rate = interval;
	mpu3050_write(data, MPU3050_SMPLRT_DIV, interval - 1);

	if (data->client->irq)
		enable_irq(data->client->irq);
	else
		queue_delayed_work(data->wq, &data->d_work, 0);

	return count;
}

#ifdef MPU3050_DEBUG
static ssize_t mpu3050_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);
	unsigned i, n, reg_count;
	uint8_t value;

	reg_count = sizeof(mpu3050_regs) / sizeof(mpu3050_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		mpu3050_read_transfer(data, mpu3050_regs[i].reg, &value, 1);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       mpu3050_regs[i].name,
			       value);
	}

	return n;
}

static ssize_t mpu3050_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);
	unsigned i, reg_count, value;
	int error = 0;
	char name[30];

	if (count >= 30) {
		pr_err("%s:input too long\n", __func__);
		return -1;
	}

	if (sscanf(buf, "%s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -1;
	}

	reg_count = sizeof(mpu3050_regs) / sizeof(mpu3050_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, mpu3050_regs[i].name)) {
			if (mpu3050_regs[i].writeable) {
				error = mpu3050_write(data,
					mpu3050_regs[i].reg,
					value);
				if (error) {
					pr_err("%s:Failed to write %s\n",
						__func__, name);
					return -1;
				}
			} else {
				pr_err("%s:Register %s is not writeable\n",
						__func__, name);
					return -1;
			}
			return count;
		}
	}

	pr_err("%s:no such register %s\n", __func__, name);
	return -1;
}

static DEVICE_ATTR(registers, S_IWUSR | S_IRUGO,
		mpu3050_registers_show, mpu3050_registers_store);
#endif

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		mpu3050_show_attr_enable, mpu3050_store_attr_enable);

static DEVICE_ATTR(delay, S_IWUSR | S_IRUGO,
		mpu3050_show_attr_delay, mpu3050_store_attr_delay);

static struct attribute *mpu3050_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
#ifdef MPU3050_DEBUG
	&dev_attr_registers.attr,
#endif
	NULL
};

static const struct attribute_group mpu3050_attr_group = {
	.attrs = mpu3050_attrs,
};

static int __devinit mpu3050_driver_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct mpu3050gyro_platform_data *pdata = client->dev.platform_data;
	struct mpu3050_gyro_data *data;
	int ret = 0;

	pr_info("%s: Enter\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Platform data not found\n", __func__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct mpu3050_gyro_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->pdata = pdata;
	data->client = client;
	data->enable = 0;
	i2c_set_clientdata(client, data);

	data->input_dev = input_allocate_device();
	if (!data->input_dev) {
		ret = -ENOMEM;
		dev_err(&data->client->dev,
			"Failed to allocate input device\n");
		goto input_device_error;
	}

	data->input_dev->name = "mpu3050";
	data->input_dev->id.bustype = BUS_I2C;

	data->input_dev->dev.parent = &data->client->dev;
	input_set_drvdata(data->input_dev, data);

	input_set_capability(data->input_dev, EV_REL, REL_RX);
	input_set_capability(data->input_dev, EV_REL, REL_RY);
	input_set_capability(data->input_dev, EV_REL, REL_RZ);


	ret = input_register_device(data->input_dev);
	if (ret) {
		dev_err(&data->client->dev,
			"Unable to register input device\n");
		input_free_device(data->input_dev);
		goto register_device_error;
	}

	mutex_init(&data->mutex);
	if (data->client->irq) {
		ret = request_threaded_irq(data->client->irq, NULL,
					mpu3050_thread_irq,
					data->pdata->irq_flags,
					data->client->name, data);
		if (ret < 0) {
			dev_err(&data->client->dev,
				"request_threaded_irq failed\n");
			goto irq_request_error;
		}
		disable_irq_nosync(data->client->irq);
	} else {
		data->def_poll_rate = data->pdata->default_poll_rate;
		INIT_DELAYED_WORK(&data->d_work,
			(void *)mpu3050_device_work);

		data->wq = create_singlethread_workqueue("mpu3050");
		if (!data->wq) {
			ret = -ENOMEM;
			goto workqueue_error;
		}
	}

	mpu3050_device_hw_init(data);

	ret = sysfs_create_group(&client->dev.kobj, &mpu3050_attr_group);
	if (ret)
		goto group_create_error;

	return 0;

group_create_error:
	if (data->client->irq)
		free_irq(data->client->irq, data);
	else
		destroy_workqueue(data->wq);
workqueue_error:
irq_request_error:
	mutex_destroy(&data->mutex);
	input_unregister_device(data->input_dev);
register_device_error:
input_device_error:
	kfree(data);
	return ret;
}

static int __devexit mpu3050_driver_remove(struct i2c_client *client)
{
	struct mpu3050_gyro_data *data = i2c_get_clientdata(client);

	if (data->enable) {
		if (data->client->irq)
			disable_irq_nosync(data->client->irq);
		else
			cancel_delayed_work_sync(&data->d_work);
	}

	if (data->client->irq)
		free_irq(data->client->irq, data);
	else
		destroy_workqueue(data->wq);

	sysfs_remove_group(&client->dev.kobj, &mpu3050_attr_group);
	input_unregister_device(data->input_dev);
	mutex_destroy(&data->mutex);
	i2c_set_clientdata(client, NULL);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
static int mpu3050_driver_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->mutex);
	if (!data->enable) {
		mutex_unlock(&data->mutex);
		return 0;
	}

	data->enable = 0;
	mutex_unlock(&data->mutex);
	data->wakeup_flag = 1;

	if (data->client->irq)
		disable_irq_nosync(data->client->irq);
	else
		cancel_delayed_work_sync(&data->d_work);

	mpu3050_device_sleep_wakeup(data, SLEEP);

	return 0;
}

static int mpu3050_driver_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct mpu3050_gyro_data *data = platform_get_drvdata(pdev);

	mutex_lock(&data->mutex);
	if (data->enable) {
		mutex_unlock(&data->mutex);
		return 0;
	}

	if (!data->wakeup_flag) {
		mutex_unlock(&data->mutex);
		return 0;
	}

	data->enable = 1;
	mutex_unlock(&data->mutex);
	data->wakeup_flag = 0;

	mpu3050_device_sleep_wakeup(data, WAKEUP);

	if (data->client->irq)
		enable_irq(data->client->irq);
	else
		queue_delayed_work(data->wq, &data->d_work, 0);

	return 0;
}

static const struct dev_pm_ops mpu3050_pm_ops = {
	.suspend = mpu3050_driver_suspend,
	.resume = mpu3050_driver_resume,
};
#endif

static const struct i2c_device_id mpu3050_idtable[] = {
	{ DEVICE_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, mpu3050_idtable);

static struct i2c_driver mpu3050_driver = {
	.probe		= mpu3050_driver_probe,
	.remove		= mpu3050_driver_remove,
	.id_table	= mpu3050_idtable,
	.driver = {
		.name = DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &mpu3050_pm_ops,
#endif
	},
};

static int __init mpu3050_driver_init(void)
{
	return i2c_add_driver(&mpu3050_driver);
}

static void __exit mpu3050_driver_exit(void)
{
	i2c_del_driver(&mpu3050_driver);
}

module_init(mpu3050_driver_init);
module_exit(mpu3050_driver_exit);

MODULE_DESCRIPTION("MPU-3050 Gyroscope Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
MODULE_AUTHOR("Jorge Bustamante <jbustamante@ti.com>");
