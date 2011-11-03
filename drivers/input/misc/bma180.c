/*
 * bma180.c
 * BMA-180 Accelerometer driver
 *
 * Copyright (C) 2010 Texas Instruments
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
 * Derived work from bma180_accl.c from Jorge Bustamante <jbustamante@ti.com>
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/i2c/bma180.h>
#include <linux/gpio.h>

#define BMA180_DEBUG 1

#define DEVICE_NAME "bma180"
#define DRIVER_NAME "bma180_accel"

#define BMA180_OFFSET_Z			0x3A
#define BMA180_OFFSET_Y			0x39
#define BMA180_OFFSET_X			0x38
#define BMA180_OFFSET_T			0x37
#define BMA180_OFFSET_LSB2		0x36
#define BMA180_OFFSET_LSB1		0x35
#define BMA180_GAIN_Z			0x34
#define BMA180_GAIN_Y			0x33
#define BMA180_GAIN_X			0x32
#define BMA180_GAIN_T			0x31
#define BMA180_TCO_Z			0x30
#define BMA180_TCO_Y			0x2F
#define BMA180_TCO_X			0x2E
#define BMA180_CD2			0x2D
#define BMA180_CD1			0x2C
#define BMA180_SLOPE_TH			0x2B
#define BMA180_HIGH_TH			0x2A
#define BMA180_LOW_TH			0x29
#define BMA180_TAPSENS_TH		0x28
#define BMA180_HIGH_DUR			0x27
#define BMA180_LOW_DUR			0x26
#define BMA180_HIGH_LOW_INFO		0x25
#define BMA180_SLOPE_TAPSENS_INFO	0x24
#define BMA180_HY			0x23
#define BMA180_CTRL_REG4		0x22
#define BMA180_CTRL_REG3		0x21
#define BMA180_BW_TCS			0x20
#define BMA180_RESET			0x10
#define BMA180_CTRL_REG2		0x0F
#define BMA180_CTRL_REG1		0x0E
#define BMA180_CTRL_REG0		0x0D
#define BMA180_STATUS_REG4		0x0C
#define BMA180_STATUS_REG3		0x0B
#define BMA180_STATUS_REG2		0x0A
#define BMA180_STATUS_REG1		0x09
#define BMA180_TEMP			0x08
#define BMA180_ACC_Z_MSB		0x07
#define BMA180_ACC_Z_LSB		0x06
#define BMA180_ACC_Y_MSB		0x05
#define BMA180_ACC_Y_LSB		0x04
#define BMA180_ACC_X_MSB		0x03
#define BMA180_ACC_X_LSB		0x02
#define BMA180_VERSION			0x01
#define BMA180_CHIP_ID			0x00

struct bma180_accel_data {
	struct bma180accel_platform_data *pdata;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct delayed_work wq;
	struct mutex mutex;

	uint32_t def_poll_rate;
};

static uint32_t accl_debug;
module_param_named(bma180_debug, accl_debug, uint, 0664);

static int g_range_table[7] = {
	1000,
	1500,
	2000,
	3000,
	4000,
	8000,
	16000,
};

/* interval between samples for the different rates, in msecs */
static const unsigned int bma180_measure_interval[] = {
	1000 / 10,  1000 / 20,  1000 / 40,  1000 / 75,
	1000 / 150, 1000 / 300, 1000 / 600, 1000 / 1200
};

#ifdef BMA180_DEBUG
struct bma180_reg {
	const char *name;
	uint8_t reg;
	int writeable;
} bma180_regs[] = {
	{ "CHIP_ID",		BMA180_CHIP_ID, 0 },
	{ "VERSION",		BMA180_VERSION, 0 },
	{ "X_LSB",		BMA180_ACC_X_LSB, 0 },
	{ "X_MSB",		BMA180_ACC_X_MSB, 0 },
	{ "Y_LSB",		BMA180_ACC_Y_LSB, 0 },
	{ "Y_MSB",		BMA180_ACC_Y_MSB, 0 },
	{ "Z_LSB",		BMA180_ACC_Z_LSB, 0 },
	{ "Z_MSB",		BMA180_ACC_Z_MSB, 0 },
	{ "TEMP",		BMA180_TEMP, 0 },
	{ "STATUS1",		BMA180_STATUS_REG1, 0 },
	{ "STATUS2",		BMA180_STATUS_REG2, 0 },
	{ "STATUS3",		BMA180_STATUS_REG3, 0 },
	{ "STATUS4",		BMA180_STATUS_REG4, 0 },
	{ "CTRL0",		BMA180_CTRL_REG0, 1 },
	{ "CTRL1",		BMA180_CTRL_REG1, 1 },
	{ "CTRL2",		BMA180_CTRL_REG2, 1 },
	{ "RESET",		BMA180_RESET, 1 },
	{ "BW_TCS",		BMA180_BW_TCS, 1 },
	{ "CTRL3",		BMA180_CTRL_REG3, 1 },
	{ "CTRL4",		BMA180_CTRL_REG4, 1 },
	{ "HY",			BMA180_HY, 1 },
	{ "TAP_INFO",		BMA180_SLOPE_TAPSENS_INFO, 1 },
	{ "HI_LOW_INFO",	BMA180_HIGH_LOW_INFO, 1 },
	{ "LOW_DUR",		BMA180_LOW_DUR, 1 },
	{ "HIGH_DUR",		BMA180_HIGH_DUR, 1 },
	{ "TAP_THRESH",		BMA180_TAPSENS_TH, 1 },
	{ "LOW_THRESH",		BMA180_LOW_TH, 1 },
	{ "HIGH_THRESH",	BMA180_HIGH_TH, 1 },
	{ "SLOPE_THRESH",       BMA180_SLOPE_TH, 1 },
	{ "CD1",		BMA180_CD1, 1 },
	{ "CD2",		BMA180_CD2, 1 },
	{ "TCO_X",		BMA180_TCO_X, 1 },
	{ "TCO_Y",		BMA180_TCO_Y, 1 },
	{ "TCO_Z",		BMA180_TCO_Z, 1 },
	{ "GAIN_T",		BMA180_GAIN_T, 1 },
	{ "GAIN_X",		BMA180_GAIN_X, 1 },
	{ "GAIN_Y",		BMA180_GAIN_Y, 1 },
	{ "GAIN_Z",		BMA180_GAIN_Z, 1 },
	{ "OFFSET_LSB1",	BMA180_OFFSET_LSB1, 1 },
	{ "OFFSET_LSB2",	BMA180_OFFSET_LSB2, 1 },
	{ "OFFSET_T",		BMA180_OFFSET_T, 1 },
	{ "OFFSET_X",		BMA180_OFFSET_X, 1 },
	{ "OFFSET_Y",		BMA180_OFFSET_Y, 1 },
	{ "OFFSET_Z",		BMA180_OFFSET_Z, 1 },
};
#endif

static int bma180_write(struct bma180_accel_data *data, u8 reg, u8 val)
{
	int ret = 0;

	mutex_lock(&data->mutex);
	ret = i2c_smbus_write_byte_data(data->client, reg, val);
	if (ret < 0)
		dev_err(&data->client->dev,
			"i2c_smbus_write_byte_data failed\n");
	mutex_unlock(&data->mutex);

	return ret;
}

static int bma180_read_transfer(struct bma180_accel_data *data,
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

static int bma180_accel_device_hw_set_bandwidth(struct bma180_accel_data *data,
					int bandwidth)
{
	uint8_t reg_val;
	uint8_t int_val;

	bma180_read_transfer(data, BMA180_CTRL_REG3, &int_val, 1);
	bma180_write(data, BMA180_CTRL_REG3, 0x00);
	bma180_read_transfer(data, BMA180_BW_TCS, &reg_val, 1);
	reg_val = (reg_val & 0x0F) | ((bandwidth << 4) & 0xF0);
	bma180_write(data, BMA180_BW_TCS, reg_val);
	msleep(10);
	bma180_write(data, BMA180_CTRL_REG3, int_val);

	return 0;
}

static void bma180_accel_device_sleep(struct bma180_accel_data *data)
{
	uint8_t reg_val;

	bma180_read_transfer(data, BMA180_CTRL_REG0, &reg_val, 1);
	reg_val |= BMA180_SLEEP;
	bma180_write(data, BMA180_CTRL_REG0, reg_val);
}

static void bma180_accel_device_wakeup(struct bma180_accel_data *data)
{
	uint8_t reg_val;

	bma180_read_transfer(data, BMA180_CTRL_REG0, &reg_val, 1);
	reg_val &= ~BMA180_SLEEP;
	bma180_write(data, BMA180_CTRL_REG0, reg_val);
	msleep(10);
}

static int bma180_accel_data_ready(struct bma180_accel_data *data)
{
	int ret;
	uint8_t data_val_h, data_val_l;
	short int x = 0;
	short int y = 0;
	short int z = 0;
	uint8_t data_buffer[6];

	ret = bma180_read_transfer(data, BMA180_ACC_X_LSB, data_buffer, 6);
	if (ret != 0) {
		dev_err(&data->client->dev,
			"bma180_read_data_ready failed\n");
		return -1;
	}
	data_val_l = data_buffer[0];
	data_val_h = data_buffer[1];
	if (accl_debug)
		pr_info("%s: X low 0x%X X high 0x%X\n",
		__func__, data_val_l, data_val_h);
	x = ((data_val_h << 8) | data_val_l);
	x = (x >> 2);
	x = x * g_range_table[data->pdata->g_range]/data->pdata->bit_mode;

	data_val_l = data_buffer[2];
	data_val_h = data_buffer[3];
	if (accl_debug)
		pr_info("%s: Y low 0x%X Y high 0x%X\n",
		__func__, data_val_l, data_val_h);

	y = ((data_val_h << 8) | data_val_l);
	y = (y >> 2);
	y = y * g_range_table[data->pdata->g_range]/data->pdata->bit_mode;

	data_val_l = data_buffer[4];
	data_val_h = data_buffer[5];
	if (accl_debug)
		pr_info("%s: Z low 0x%X Z high 0x%X\n",
		__func__, data_val_l, data_val_h);

	z = ((data_val_h << 8) | data_val_l);
	z = (z >> 2);
	z = z * g_range_table[data->pdata->g_range]/data->pdata->bit_mode;

	if (accl_debug)
		pr_info("%s: X: 0x%X Y: 0x%X Z: 0x%X\n",
		__func__, x, y, z);

	input_report_abs(data->input_dev, ABS_X, x);
	input_report_abs(data->input_dev, ABS_Y, y);
	input_report_abs(data->input_dev, ABS_Z, z);
	input_sync(data->input_dev);

	return 0;
}

static irqreturn_t bma180_accel_thread_irq(int irq, void *dev_data)
{
	struct bma180_accel_data *data = (struct bma180_accel_data *) dev_data;

	if (!data->client->irq)
		schedule_delayed_work(&data->wq, 0);
	else
		bma180_accel_data_ready(data);

	return IRQ_HANDLED;
}

static void bma180_accel_device_worklogic(struct work_struct *work)
{
	struct bma180_accel_data *data = container_of((struct delayed_work *)work,
				struct bma180_accel_data, wq);

	if (data->pdata->mode) {
		bma180_accel_data_ready(data);
		if (!data->client->irq)
			schedule_delayed_work(&data->wq,
				msecs_to_jiffies(data->def_poll_rate));
	}

}

static ssize_t bma180_show_attr_enable(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", data->pdata->mode);
}

static ssize_t bma180_store_attr_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error, enable;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;
	enable = !!val;

	if (data->pdata->mode == enable)
		return count;

	if (enable) {
		bma180_accel_device_wakeup(data);
		if (!data->client->irq)
			schedule_delayed_work(&data->wq, 0);
	} else {
		bma180_accel_device_sleep(data);
		if (!data->client->irq)
			cancel_delayed_work_sync(&data->wq);
	}

	data->pdata->mode = enable;

	return count;
}

static ssize_t bma180_show_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", data->def_poll_rate);
}

static ssize_t bma180_store_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);
	unsigned long interval;
	int error;
	int i = 0;

	error = strict_strtoul(buf, 0, &interval);
	if (error)
		return error;

	if (interval < 0)
		return -EINVAL;

	if (!data->client->irq)
		cancel_delayed_work_sync(&data->wq);

	if (interval >=
		bma180_measure_interval[BMA_BW_10HZ])
		i = BMA_BW_10HZ;
	else if (interval >=
		bma180_measure_interval[BMA_BW_20HZ])
		i = BMA_BW_20HZ;
	else if (interval >=
		bma180_measure_interval[BMA_BW_40HZ])
		i = BMA_BW_40HZ;
	else if (interval >=
		bma180_measure_interval[BMA_BW_75HZ])
		i = BMA_BW_75HZ;
	else if (interval >=
		bma180_measure_interval[BMA_BW_150HZ])
		i = BMA_BW_150HZ;
	else if (interval >=
		bma180_measure_interval[BMA_BW_300HZ])
		i = BMA_BW_300HZ;
	else if (interval >=
		bma180_measure_interval[BMA_BW_600HZ])
		i = BMA_BW_600HZ;
	else
		i = BMA_BW_1200HZ;

	data->def_poll_rate = interval;
	bma180_accel_device_hw_set_bandwidth(data, i);

	if (!data->client->irq)
		schedule_delayed_work(&data->wq, 0);

	return count;

}
#ifdef BMA180_DEBUG
static ssize_t bma180_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);
	unsigned i, n, reg_count;
	uint8_t value;

	reg_count = sizeof(bma180_regs) / sizeof(bma180_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		bma180_read_transfer(data, bma180_regs[i].reg, &value, 1);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       bma180_regs[i].name,
			       value);
	}

	return n;
}

static ssize_t bma180_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);
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

	reg_count = sizeof(bma180_regs) / sizeof(bma180_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, bma180_regs[i].name)) {
			if (bma180_regs[i].writeable) {
				error = bma180_write(data,
					bma180_regs[i].reg,
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
		bma180_registers_show, bma180_registers_store);
#endif
static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		bma180_show_attr_enable, bma180_store_attr_enable);

static DEVICE_ATTR(delay, S_IWUSR | S_IRUGO,
		bma180_show_attr_delay, bma180_store_attr_delay);

static struct attribute *bma180_accel_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
#ifdef BMA180_DEBUG
	&dev_attr_registers.attr,
#endif
	NULL
};

static const struct attribute_group bma180_accel_attr_group = {
	.attrs = bma180_accel_attrs,
};

static int bma180_accel_device_hw_reset(struct bma180_accel_data *data)
{
	/* write 0xB6 to this register to do a soft-reset */
	bma180_write(data, BMA180_RESET, 0xB6);

	return 0;
}

static int bma180_accel_device_hw_reset_int(struct bma180_accel_data *data)
{
	uint8_t reg_val;

	bma180_read_transfer(data, BMA180_CTRL_REG0, &reg_val, 1);
	reg_val |= 0x40;
	bma180_write(data, BMA180_CTRL_REG0, reg_val);

	return 0;
}

static int bma180_accel_device_hw_set_grange(struct bma180_accel_data *data,
					int grange)
{
	uint8_t reg_val;

	bma180_read_transfer(data, BMA180_OFFSET_LSB1, &reg_val, 1);
	reg_val = (reg_val & 0xF1) | ((grange << 1) & 0x0E);
	bma180_write(data, BMA180_OFFSET_LSB1, reg_val);

	return 0;
}

static int bma180_accel_device_hw_set_smp_skip(struct bma180_accel_data *data,
					int smp_skip)
{
	uint8_t reg_val;

	bma180_read_transfer(data, BMA180_OFFSET_LSB1, &reg_val, 1);
	reg_val = (reg_val & 0xFE) | ((smp_skip << 0) & 0x01);
	bma180_write(data, BMA180_OFFSET_LSB1, reg_val);

	return 0;
}

static int bma180_accel_device_hw_set_mode(struct bma180_accel_data *data,
					int mode)
{
	uint8_t reg_val;

	bma180_read_transfer(data, BMA180_TCO_Z, &reg_val, 1);
	reg_val = (reg_val & 0xFC) | ((mode << 0) & 0x03);
	bma180_write(data, BMA180_TCO_Z, reg_val);

	return 0;
}

static int bma180_accel_device_hw_set_12bits(struct bma180_accel_data *data,
					int mode)
{
	uint8_t reg_val;

	bma180_read_transfer(data, BMA180_OFFSET_T, &reg_val, 1);
	reg_val = (reg_val & 0xFE) | ((mode << 0) & 0x01);
	bma180_write(data, BMA180_OFFSET_T, reg_val);

	return 0;
}

static int bma180_accel_device_hw_init(struct bma180_accel_data *data)
{
	int ret = 0;

	bma180_accel_device_hw_reset(data);
	msleep(1);
	bma180_write(data, BMA180_CTRL_REG0, data->pdata->ctrl_reg0);
	bma180_write(data, BMA180_CTRL_REG3, 0x00);
	bma180_write(data, BMA180_HIGH_LOW_INFO, 0x00);
	bma180_write(data, BMA180_SLOPE_TAPSENS_INFO, 0x00);
	bma180_write(data, BMA180_GAIN_Y, 0xA9);
	bma180_write(data, BMA180_HIGH_DUR, 0x00);
	bma180_accel_device_hw_set_grange(data, data->pdata->g_range);
	bma180_accel_device_hw_set_bandwidth(data, data->pdata->bandwidth);
	bma180_accel_device_hw_set_mode(data, data->pdata->mode);
	bma180_accel_device_hw_set_smp_skip(data, data->pdata->smp_skip);
	bma180_accel_device_hw_set_12bits(data, data->pdata->bit_mode);
	bma180_write(data, BMA180_CTRL_REG3, 0x02);

	bma180_accel_device_hw_reset_int(data);

	return ret;
}


static int __devinit bma180_accel_driver_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct bma180accel_platform_data *pdata = client->dev.platform_data;
	struct bma180_accel_data *data;
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

	/* alloc memory for data structure */
	data = kzalloc(sizeof(struct bma180_accel_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto error;
	}
	data->pdata = pdata;
	data->client = client;
	data->def_poll_rate = pdata->def_poll_rate;
	i2c_set_clientdata(client, data);

	data->input_dev = input_allocate_device();
	if (data->input_dev == NULL) {
		ret = -ENOMEM;
		dev_err(&data->client->dev,
			"Failed to allocate input device\n");
		goto error;
	}

	INIT_DELAYED_WORK(&data->wq, bma180_accel_device_worklogic);

	mutex_init(&data->mutex);

	data->input_dev->name = "bma180";
	data->input_dev->id.bustype = BUS_I2C;

	__set_bit(EV_ABS, data->input_dev->evbit);
	input_set_abs_params(data->input_dev, ABS_X,
				-g_range_table[data->pdata->g_range],
				g_range_table[data->pdata->g_range],
				data->pdata->fuzz_x, 0);
	input_set_abs_params(data->input_dev, ABS_Y,
				-g_range_table[data->pdata->g_range],
				g_range_table[data->pdata->g_range],
				data->pdata->fuzz_y, 0);
	input_set_abs_params(data->input_dev, ABS_Z,
				-g_range_table[data->pdata->g_range],
				g_range_table[data->pdata->g_range],
				data->pdata->fuzz_z, 0);

	data->input_dev->dev.parent = &data->client->dev;
	input_set_drvdata(data->input_dev, data);

	ret = input_register_device(data->input_dev);
	if (ret) {
		dev_err(&data->client->dev,
			"Unable to register input device\n");
	}

	if (data->client->irq) {
		ret = request_threaded_irq(data->client->irq, NULL,
					bma180_accel_thread_irq,
					IRQF_TRIGGER_RISING | IRQF_ONESHOT,
					data->client->name, data);
		if (ret < 0) {
			dev_err(&data->client->dev,
				"request_threaded_irq failed\n");
				goto error_1;
		}
	}

	ret = bma180_accel_device_hw_init(data);
	if (ret)
		goto error_1;

	ret = sysfs_create_group(&client->dev.kobj, &bma180_accel_attr_group);
	if (ret)
		goto error_1;

	return 0;

error_1:
	input_free_device(data->input_dev);
	mutex_destroy(&data->mutex);
	kfree(data);
error:
	return ret;
}

static int __devexit bma180_accel_driver_remove(struct i2c_client *client)
{
	struct bma180_accel_data *data = i2c_get_clientdata(client);
	int ret = 0;

	sysfs_remove_group(&client->dev.kobj, &bma180_accel_attr_group);

	if (data->client->irq)
		free_irq(data->client->irq, data);

	cancel_delayed_work_sync(&data->wq);

	if (data->input_dev)
		input_free_device(data->input_dev);

	i2c_set_clientdata(client, NULL);
	kfree(data);

	return ret;
}


#ifdef CONFIG_PM
static int bma180_accel_driver_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);

	if (data->pdata->mode)
		bma180_accel_device_sleep(data);

	return 0;
}

static int bma180_accel_driver_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bma180_accel_data *data = platform_get_drvdata(pdev);

	if (data->pdata->mode)
		bma180_accel_device_wakeup(data);

	return 0;
}

static const struct dev_pm_ops bma180_pm_ops = {
	.suspend = bma180_accel_driver_suspend,
	.resume = bma180_accel_driver_resume,
};
#endif

static const struct i2c_device_id bma180_accel_idtable[] = {
	{ DRIVER_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, bma180_accel_idtable);

static struct i2c_driver bma180_accel_driver = {
	.probe		= bma180_accel_driver_probe,
	.remove		= bma180_accel_driver_remove,
	.id_table	= bma180_accel_idtable,
	.driver = {
		.name = DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &bma180_pm_ops,
#endif
	},
};

static int __init bma180_accel_driver_init(void)
{
	return i2c_add_driver(&bma180_accel_driver);
}

static void __exit bma180_accel_driver_exit(void)
{
	i2c_del_driver(&bma180_accel_driver);
}

module_init(bma180_accel_driver_init);
module_exit(bma180_accel_driver_exit);

MODULE_DESCRIPTION("BMA-180 Accelerometer Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
