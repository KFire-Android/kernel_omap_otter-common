/* drivers/misc/drv2667.c
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 * Copyright (C) 2008 Google, Inc.
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Derived from: vib-gpio.c
 * Additional derivation from: twl6040-vibra.c
 */

#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c/drv2667.h>
#include "../staging/android/timed_output.h"

#define DRV2667_DEBUG

#define DRV2667_ALLOWED_R_BYTES	25
#define DRV2667_ALLOWED_W_BYTES	2
#define DRV2667_MAX_RW_RETRIES	5
#define DRV2667_I2C_RETRY_DELAY 10

#define DRV2667_STANDBY		0x40
#define DRV2667_RESET		0x80

struct drv2667_data {
	struct timed_output_dev dev;
	struct i2c_client *client;
	struct hrtimer timer;

	struct drv2667_platform_data *pdata;

	int vib_power_state;
	int vib_state;
	bool powered;
};

static uint32_t haptics_debug;
module_param_named(drv2667_debug, haptics_debug, uint, 0664);

#ifdef DRV2667_DEBUG
static struct drv2667_reg {
	const char *name;
	uint8_t reg;
	int writeable;
} drv2667_regs[] = {
	{ "STATUS",		DRV2667_STATUS, 0 },
	{ "CONTROL1",		DRV2667_CTRL_1, 1 },
	{ "CONTROL2",		DRV2667_CTRL_2, 1 },
	{ "WAVE_SEQ_0",		DRV2667_WV_SEQ_0, 1 },
	{ "WAVE_SEQ_1",		DRV2667_WV_SEQ_1, 1 },
	{ "WAVE_SEQ_2",		DRV2667_WV_SEQ_2, 1},
	{ "WAVE_SEQ_3",		DRV2667_WV_SEQ_3, 1 },
	{ "WAVE_SEQ_4",		DRV2667_WV_SEQ_4, 1 },
	{ "WAVE_SEQ_5",		DRV2667_WV_SEQ_5, 1 },
	{ "WAVE_SEQ_6",		DRV2667_WV_SEQ_6, 1 },
	{ "WAVE_SEQ_7",		DRV2667_WV_SEQ_7, 1 },
	{ "FIFO",		DRV2667_FIFO, 1 },
	{ "PAGE",		DRV2667_PAGE, 1 },
};

static struct drv2667_ram_reg {
	const char *name;
	uint8_t reg;
	int writeable;
} drv2667_ram_regs[] = {
	/* RAM Addresses */
	{ "HDR_SZ",		DRV2667_RAM_HDR_SZ, 0 },
	{ "START_ADDR_HI",	DRV2667_RAM_START_HI, 1 },
	{ "START_ADDR_LO",	DRV2667_RAM_START_LO, 1 },
	{ "STOP_ADDR_HI",	DRV2667_RAM_STOP_HI, 1 },
	{ "STOP_ADDR_LO",	DRV2667_RAM_STOP_LO, 1 },
	{ "REPEAT_COUNT",	DRV2667_RAM_REPEAT_CT, 1},
	{ "AMPLITUDE",		DRV2667_RAM_AMP, 1 },
	{ "FREQ",		DRV2667_RAM_FREQ, 1 },
	{ "DURATION",		DRV2667_RAM_DURATION, 1 },
	{ "ENVELOPE",		DRV2667_RAM_ENVELOPE, 1 },
};
#endif

static int drv2667_write_reg(struct drv2667_data *data, u8 reg,
					u8 val, int len)
{
	int err;
	int tries = 0;
	u8 buf[DRV2667_ALLOWED_W_BYTES];

	struct i2c_msg msgs[] = {
		{
		 .addr = data->client->addr,
		 .flags = data->client->flags,
		 .len = len + 1,
		 },
	};

	buf[0] = reg;
	/* TO DO: Need to find out if we can write multiple bytes at once over
	* I2C like we can with the read */
	buf[1] = val;

	msgs->buf = buf;

	do {
		err = i2c_transfer(data->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(DRV2667_I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < DRV2667_MAX_RW_RETRIES));

	if (err != 1) {
		dev_err(&data->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int drv2667_read_reg(struct drv2667_data *data, u8 reg, u8 *buf, int len)
{
	int err;
	int tries = 0;
	u8 reg_buf[DRV2667_ALLOWED_R_BYTES];

	struct i2c_msg msgs[] = {
		{
		 .addr = data->client->addr,
		 .flags = data->client->flags,
		 .len = 1,
		 },
		{
		 .addr = data->client->addr,
		 .flags = (data->client->flags | I2C_M_RD),
		 .len = len,
		 .buf = buf,
		 },
	};
	reg_buf[0] = reg;
	msgs->buf = reg_buf;

	do {
		err = i2c_transfer(data->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(DRV2667_I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < DRV2667_MAX_RW_RETRIES));

	if (err != 2) {
		dev_err(&data->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int drv2667_set_initial_pattern(struct drv2667_data *data)
{
	int i;

	drv2667_write_reg(data, DRV2667_CTRL_2, 0x00, 1);
	drv2667_write_reg(data, DRV2667_CTRL_1, 0x00, 1);

	if (haptics_debug)
		pr_info("%s:Found %i number of patterns\n", __func__,
			data->pdata->drv2667_pattern_data.num_of_patterns);

	/* Write the patterns into RAM */
	for (i = 1; i <= data->pdata->drv2667_pattern_data.num_of_patterns; i++) {
		struct drv2667_pattern_data *pattern =
			&data->pdata->drv2667_pattern_data.pattern_data[i - 1];

		drv2667_write_reg(data, DRV2667_WV_SEQ_0, pattern->wave_seq, 1);
		drv2667_write_reg(data, DRV2667_PAGE, pattern->page_num, 1);
		drv2667_write_reg(data, DRV2667_RAM_HDR_SZ,
			pattern->hdr_size, 1);
		drv2667_write_reg(data, DRV2667_RAM_START_HI,
			pattern->start_addr_upper, 1);
		drv2667_write_reg(data, DRV2667_RAM_START_LO,
			pattern->start_addr_lower, 1);
		drv2667_write_reg(data, DRV2667_RAM_STOP_HI,
			pattern->stop_addr_upper, 1);
		drv2667_write_reg(data, DRV2667_RAM_STOP_LO,
			pattern->stop_addr_lower, 1);
		drv2667_write_reg(data, DRV2667_RAM_REPEAT_CT,
			pattern->repeat_count, 1);
		drv2667_write_reg(data, DRV2667_RAM_AMP,
			pattern->amplitude, 1);
		drv2667_write_reg(data, DRV2667_RAM_FREQ,
			pattern->frequency, 1);
		drv2667_write_reg(data, DRV2667_RAM_DURATION,
			pattern->duration, 1);
		drv2667_write_reg(data, DRV2667_RAM_ENVELOPE,
			pattern->envelope, 1);
	}

	drv2667_write_reg(data, DRV2667_PAGE, 0x0, 1);
	return 0;
}

static int drv2667_get_time(struct timed_output_dev *dev)
{
/*	struct drv2667_data *data =
		container_of(dev, struct drv2667_data, dev);*/

	return 0;
}

static void drv2667_enable(struct timed_output_dev *dev, int value)
{
	struct drv2667_data *data = container_of(dev, struct drv2667_data, dev);
	int cycles = value / 5; /* Time in milli seconds */
	u8 read_buf[2];

	if (haptics_debug)
		pr_info("%s:Requested %d vibration time means %d cycles\n",
			__func__, value, cycles);

	drv2667_write_reg(data, DRV2667_PAGE, 0x01, 1);
	drv2667_write_reg(data, DRV2667_RAM_DURATION, cycles, 1);
	drv2667_write_reg(data, DRV2667_PAGE, 0x00, 1);
	drv2667_write_reg(data, DRV2667_CTRL_2, 0x01, 1);
	if (haptics_debug)
		drv2667_read_reg(data, DRV2667_STATUS, read_buf, 1);
}

#ifdef DRV2667_DEBUG
static ssize_t drv2667_ram_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv2667_data *data = platform_get_drvdata(pdev);
	unsigned i, reg_count;
	unsigned n = -1;
	u8 read_buf[2];

	drv2667_read_reg(data, DRV2667_PAGE, read_buf, 1);
	if (read_buf[0] == 0) {
		pr_err("%s:Please set the correct RAM PAGE\n", __func__);
		return n;
	}

	reg_count = sizeof(drv2667_ram_regs) / sizeof(drv2667_ram_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		drv2667_read_reg(data, drv2667_ram_regs[i].reg, read_buf, 1);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       drv2667_ram_regs[i].name,
			       read_buf[0]);
	}

	return n;
}

static ssize_t drv2667_ram_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv2667_data *data = platform_get_drvdata(pdev);
	unsigned i, reg_count, value;
	int error = 0;
	char name[30];
	u8 read_buf[2];

	drv2667_read_reg(data, DRV2667_PAGE, read_buf, 1);
	if (read_buf[0] == 0) {
		pr_err("%s:Please set the correct RAM PAGE\n", __func__);
		return count;
	}

	if (count >= 30) {
		pr_err("%s:input too long\n", __func__);
		return -1;
	}

	if (sscanf(buf, "%s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -1;
	}

	reg_count = sizeof(drv2667_ram_regs) / sizeof(drv2667_ram_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, drv2667_ram_regs[i].name)) {
			if (drv2667_ram_regs[i].writeable) {
				error = drv2667_write_reg(data,
						drv2667_ram_regs[i].reg, value, 1);
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

static ssize_t drv2667_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv2667_data *data = platform_get_drvdata(pdev);
	unsigned i, reg_count;
	unsigned n = -1;
	u8 read_buf[2];

	drv2667_read_reg(data, DRV2667_PAGE, read_buf, 1);
	if (read_buf[0] > 0) {
		pr_err("%s:Set PAGE back to 0 current page %i\n",
			__func__, read_buf[0]);
		return n;
	}

	reg_count = sizeof(drv2667_regs) / sizeof(drv2667_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		drv2667_read_reg(data, drv2667_regs[i].reg, read_buf, 1);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       drv2667_regs[i].name,
			       read_buf[0]);
	}

	return n;
}

static ssize_t drv2667_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv2667_data *data = platform_get_drvdata(pdev);
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

	reg_count = sizeof(drv2667_regs) / sizeof(drv2667_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, drv2667_regs[i].name)) {
			if (drv2667_regs[i].writeable) {
				error = drv2667_write_reg(data,
						drv2667_regs[i].reg, value, 1);
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
		drv2667_registers_show, drv2667_registers_store);
static DEVICE_ATTR(ram_registers, S_IWUSR | S_IRUGO,
		drv2667_ram_registers_show, drv2667_ram_registers_store);
#endif

static struct attribute *drv2667_attrs[] = {
#ifdef DRV2667_DEBUG
	&dev_attr_registers.attr,
	&dev_attr_ram_registers.attr,
#endif
	NULL
};

static const struct attribute_group drv2667_attr_group = {
	.attrs = drv2667_attrs,
};

static int __devinit drv2667_driver_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct drv2667_platform_data *pdata = client->dev.platform_data;
	struct drv2667_data *data;
	int ret = 0;

	pr_info("%s:Enter\n", __func__);

	if (!pdata) {
		ret = -EBUSY;
		goto err0;
	}

	data = kzalloc(sizeof(struct drv2667_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto err0;
	}

	data->pdata = pdata;
	data->client = client;
	data->dev.name = "vibrator";
	data->dev.get_time = drv2667_get_time;
	data->dev.enable = drv2667_enable;
	ret = timed_output_dev_register(&data->dev);
	if (ret < 0) {
		pr_err("%s:Cannot register time output dev error %i\n",
			__func__, ret);
		goto err1;
	}

	i2c_set_clientdata(client, data);
	drv2667_set_initial_pattern(data);
	ret = sysfs_create_group(&client->dev.kobj, &drv2667_attr_group);
	if (ret) {
		pr_err("%s:Cannot create sysfs group\n", __func__);
		goto err2;
	}


	drv2667_enable(&data->dev, data->pdata->initial_vibrate);
	pr_info("%s:Exit\n", __func__);
	return 0;

err2:
	timed_output_dev_unregister(&data->dev);
err1:
	kfree(data);
err0:
	return ret;
}

static int __devexit drv2667_driver_remove(struct i2c_client *client)
{
	struct drv2667_data *data = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &drv2667_attr_group);
	timed_output_dev_unregister(&data->dev);
	kfree(data);

	return 0;
}

/* TO DO: Need to run through the power management APIs to make sure we
 * do not break power management and the sensor */
#ifdef CONFIG_PM
static int drv2667_driver_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv2667_data *data = platform_get_drvdata(pdev);
	u8 read_buf[2];

	drv2667_read_reg(data, DRV2667_CTRL_2, read_buf, 1);
	drv2667_write_reg(data, DRV2667_CTRL_2,
		(read_buf[0] | DRV2667_STANDBY), 1);

	return 0;
}

static int drv2667_driver_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct drv2667_data *data = platform_get_drvdata(pdev);
	u8 read_buf[2];

	drv2667_read_reg(data, DRV2667_CTRL_2, read_buf, 1);
	drv2667_write_reg(data, DRV2667_CTRL_2,
		(read_buf[0] & ~DRV2667_STANDBY), 1);

	return 0;
}

static const struct dev_pm_ops drv2667_pm_ops = {
	.suspend = drv2667_driver_suspend,
	.resume = drv2667_driver_resume,
};
#endif

static const struct i2c_device_id drv2667_idtable[] = {
	{DRV2667_NAME, 0},
	{ },
};

MODULE_DEVICE_TABLE(i2c, drv2667_idtable);

static struct i2c_driver drv2667_driver = {
	.probe		= drv2667_driver_probe,
	.remove		= drv2667_driver_remove,
	.id_table	= drv2667_idtable,
	.driver = {
		.name = DRV2667_NAME,
#ifdef CONFIG_PM
		.pm = &drv2667_pm_ops,
#endif
	},
};

static int __init drv2667_init(void)
{
	return i2c_add_driver(&drv2667_driver);
}

static void __exit drv2667_exit(void)
{
	i2c_del_driver(&drv2667_driver);
}

module_init(drv2667_init);
module_exit(drv2667_exit);

MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com>");
MODULE_DESCRIPTION("DRV2667 Piezoelectric Driver");
MODULE_LICENSE("GPL");
