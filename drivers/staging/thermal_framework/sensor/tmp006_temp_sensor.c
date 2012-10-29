/*
 * tmp006 Temperature sensor driver file
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Based on tmp102_temp_sensor.c written by:
 * Author: Steven King <sfking@fdwdc.com>
 * The original code was for tmp102, I2C based temperature sensor.
 * This code is for for tmp006, I2C based IR temperature sensor.
 * Author: Radhesh, Fadnis" <radhesh.fadnis@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/i2c/tmp006.h>
#include <linux/thermal_framework.h>

#include <plat/common.h>

#define TMP006_VOLT_REG 0x00
#define TMP006_TEMP_REG 0x01
#define TMP006_CONF_REG 0x02
#define TMP006_MFG_ID_REG 0xFE
#define TMP006_DEV_ID_REG 0xFF

/* Bit fields of Config Register */
#define TMP006_CONF_RST 0x8000
#define TMP006_CONF_MOD_MASK 0x7000
#define TMP006_CONF_MOD_SHIFT 12
#define TMP006_CONF_CR_MASK 0x0E00
#define TMP006_CONF_CR_SHIFT 9
#define TMP006_CONF_EN_DRDY_PIN 0x0100
#define TMP006_CONF_DRDY 0x0080

#define TMP006_CONF_MOD_SHUTDOWN 0x0
#define TMP006_CONF_MOD_ENABLE 0x7

#define TMP006_TEMP_SHIFT_VAL 2 /* Shift by this value to get the temp */
#define TMP006_TEMP_ENG_CONV 32 /* Each LSB is 1/32 deg C */

/*
 * tmp006_temp_sensor structure
 * @iclient - I2c client pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for for reading current temp
 * @therm_fw - thermal device
 * @tmp006_sensor_work - delayed periodic work function to report temperature
 * @work_time_period - time period between two temperature updates
 * @last_update - time stamp in jiffies when the last temp was read
 * @current_temp - current sensor temp
 */
struct tmp006_temp_sensor {
	struct i2c_client *iclient;
	struct device *dev;
	struct mutex sensor_mutex;
	struct thermal_dev *therm_fw;
	struct delayed_work tmp006_sensor_work;
	int work_time_period;
	unsigned long last_update;
	int current_temp;
};

/* SMBus specifies low byte first, but the tmp006 returns high byte first,
 * so we have to swab16 the values
 */
static inline int tmp006_read_reg(struct i2c_client *client, u8 reg, u16 *data)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s: I2C read error\n", __func__);
		return ret;
	}

	*data = swab16(ret);

	return 0;
}

static inline int tmp006_write_reg(struct i2c_client *client,
				u8 reg, u16 val)
{
	return i2c_smbus_write_word_data(client, reg, swab16(val));
}

/* convert 14-bit tmp006 register value to milliCelsius */
static inline int tmp006_reg_to_mc(s16 val)
{
	return ((val >> TMP006_TEMP_SHIFT_VAL) * 1000) / TMP006_TEMP_ENG_CONV;
}

static int tmp006_read_current_temp(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);

	mutex_lock(&tmp006->sensor_mutex);
	if (time_after(jiffies, tmp006->last_update + HZ)) {
		u16 val;
		int ret = tmp006_read_reg(client, TMP006_TEMP_REG, &val);
		if (ret < 0) {
			mutex_unlock(&tmp006->sensor_mutex);
			return ret;
		}
		tmp006->current_temp = tmp006_reg_to_mc(val);
		tmp006->last_update = jiffies;
	}
	mutex_unlock(&tmp006->sensor_mutex);

	return tmp006->current_temp;
}

static int tmp006_get_temp(struct thermal_dev *tdev)
{
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);

	tmp006->therm_fw->current_temp =
			tmp006_read_current_temp(tdev->dev);

	return tmp006->therm_fw->current_temp;
}

static int tmp006_report_temp(struct thermal_dev *tdev)
{
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);

	tmp006->therm_fw->current_temp = thermal_request_temp(tdev);

	pr_debug("%s: tmp006 temp %d\n", __func__,
		tmp006->therm_fw->current_temp);

	thermal_sensor_set_temp(tmp006->therm_fw);

	return tmp006->therm_fw->current_temp;
}

/* debugfs hooks for tmp006 sensor */
static int get_current_temp(void *data, u64 *val)
{
	*val = tmp006_get_temp(data);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(tmp006_fops, get_current_temp, NULL, "%llu\n");

static int report_period_get(void *data, u64 *val)
{
	struct thermal_dev *tdev = data;
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);

	*val = tmp006->work_time_period;

	return 0;
}

static int report_period_set(void *data, u64 val)
{
	struct thermal_dev *tdev = data;
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);

	mutex_lock(&tmp006->sensor_mutex);
	tmp006->work_time_period = (int)val;
	mutex_unlock(&tmp006->sensor_mutex);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(report_period_fops, report_period_get,
						report_period_set, "%llu\n");

#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
static int tmp006_sensor_register_debug_entries(struct thermal_dev *tdev,
					struct dentry *d)
{
	/* Read Only - Debug properties of tmp006 sensor */
	(void) debugfs_create_file("current_temp",
			S_IRUGO, d, tdev,
			&tmp006_fops);

	/* Read/Write - Debug properties of tmp006 sensor */
	(void) debugfs_create_file("report_period_ms",
			S_IRUGO | S_IWUSR, d, tdev,
			&report_period_fops);
	return 0;
}
#endif

static struct thermal_dev_ops tmp006_temp_sensor_ops = {
	.report_temp = tmp006_get_temp,
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	.register_debug_entries = tmp006_sensor_register_debug_entries,
#endif
};

static void tmp006_sensor_delayed_work_fn(struct work_struct *work)
{
	struct tmp006_temp_sensor *temp_sensor =
				container_of(work, struct tmp006_temp_sensor,
					     tmp006_sensor_work.work);

	tmp006_report_temp(temp_sensor->therm_fw);
	schedule_delayed_work(&temp_sensor->tmp006_sensor_work,
			msecs_to_jiffies(temp_sensor->work_time_period));
}

static int __devinit tmp006_temp_sensor_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tmp006_temp_sensor *tmp006;
	struct tmp006_platform_data *tmp006_pd;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "adapter doesn't support SMBus word transactions\n");
		return -ENODEV;
	}

	tmp006_pd = dev_get_platdata(&client->dev);
	if (!tmp006_pd) {
		dev_err(&client->dev, "%s: Invalid platform data\n", __func__);
		return -EINVAL;
	}

	tmp006 = kzalloc(sizeof(struct tmp006_temp_sensor), GFP_KERNEL);
	if (!tmp006) {
		dev_err(&client->dev, "%s: No memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&tmp006->sensor_mutex);

	tmp006->iclient = client;
	tmp006->dev = &client->dev;

	/* Default configration of the TMP006 sensor are good.
	 * Conversion rate of 1 sec and Continuous conversion mode.
	 * If other configuration is required set it here.
	 */

	tmp006->last_update = jiffies - HZ;

	tmp006->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (tmp006->therm_fw) {
		tmp006->therm_fw->name = "tmp006_sensor";
		tmp006->therm_fw->domain_name = "case";

		tmp006->therm_fw->dev = tmp006->dev;
		tmp006->therm_fw->dev_ops = &tmp006_temp_sensor_ops;
		ret = thermal_sensor_dev_register(tmp006->therm_fw);
		if (ret)
			goto therm_dev_reg_err;
	} else {
		ret = -ENOMEM;
		goto therm_fw_alloc_err;
	}

	i2c_set_clientdata(client, tmp006);

	/* Init delayed work for Case sensor temperature */
	INIT_DELAYED_WORK(&tmp006->tmp006_sensor_work,
			  tmp006_sensor_delayed_work_fn);

	tmp006->work_time_period = tmp006_pd->update_period;
	schedule_work(&tmp006->tmp006_sensor_work.work);

	dev_info(&client->dev, "initialized\n");

	return 0;

therm_dev_reg_err:
	kfree(tmp006->therm_fw);
therm_fw_alloc_err:
	mutex_destroy(&tmp006->sensor_mutex);
	kfree(tmp006);

	return ret;
}

static int __devexit tmp006_temp_sensor_remove(struct i2c_client *client)
{
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);
	u16 config;
	int ret = 0;

	cancel_delayed_work_sync(&tmp006->tmp006_sensor_work);

	thermal_sensor_dev_unregister(tmp006->therm_fw);

	ret = tmp006_read_reg(client, TMP006_CONF_REG, &config);
	if (ret < 0)
		/* Even if the read fails, free the driver memory*/
		goto free_drv;
	/* Set the MOD bits to 0b000 to shutdown */
	config &= ~TMP006_CONF_MOD_MASK;
	config |= TMP006_CONF_MOD_SHUTDOWN << TMP006_CONF_MOD_SHIFT;
	tmp006_write_reg(client, TMP006_CONF_REG, config);

free_drv:
	kfree(tmp006->therm_fw);
	kfree(tmp006);

	return 0;
}

#ifdef CONFIG_PM
static int tmp006_temp_sensor_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u16 config;
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&tmp006->tmp006_sensor_work);

	ret = tmp006_read_reg(client, TMP006_CONF_REG, &config);
	if (ret < 0) {
		dev_err(&client->dev, "%s: tmp006 read error\n", __func__);
		return ret;
	}

	/* Set the MOD bits to 0b000 to shutdown */
	config &= ~TMP006_CONF_MOD_MASK;
	config |= TMP006_CONF_MOD_SHUTDOWN << TMP006_CONF_MOD_SHIFT;
	ret = tmp006_write_reg(client, TMP006_CONF_REG, config);
	if (ret < 0)
		dev_err(&client->dev, "%s: tmp006 write error\n", __func__);

	return 0;
}

static int tmp006_temp_sensor_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u16 config;
	struct tmp006_temp_sensor *tmp006 = i2c_get_clientdata(client);

	ret = tmp006_read_reg(client, TMP006_CONF_REG, &config);
	if (ret < 0) {
		dev_err(&client->dev, "%s: tmp006 read error\n", __func__);
		return ret;
	}

	/* Set the MOD bits to 0b111 to Continuous Coversion mode */
	config &= ~TMP006_CONF_MOD_MASK;
	config |= TMP006_CONF_MOD_ENABLE << TMP006_CONF_MOD_SHIFT;
	ret = tmp006_write_reg(client, TMP006_CONF_REG, config);
	if (ret < 0)
		dev_err(&client->dev, "%s: tmp006 write error\n", __func__);

	schedule_work(&tmp006->tmp006_sensor_work.work);
	return ret;
}
static const struct dev_pm_ops tmp006_dev_pm_ops = {
	.suspend = tmp006_temp_sensor_suspend,
	.resume = tmp006_temp_sensor_resume,
};

#define tmp006_DEV_PM_OPS (&tmp006_dev_pm_ops)
#else
#define tmp006_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id tmp006_id[] = {
	{ "tmp006_temp_sensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmp006_id);

static struct i2c_driver tmp006_driver = {
	.probe = tmp006_temp_sensor_probe,
	.remove = tmp006_temp_sensor_remove,
	.driver = {
		.name = "tmp006_temp_sensor",
		.pm = tmp006_DEV_PM_OPS,
	},
	.id_table = tmp006_id,
};

static int __init tmp006_init(void)
{
	return i2c_add_driver(&tmp006_driver);
}
module_init(tmp006_init);

static void __exit tmp006_exit(void)
{
	i2c_del_driver(&tmp006_driver);
}
module_exit(tmp006_exit);

MODULE_DESCRIPTION("tmp006 Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: tmp006");
MODULE_AUTHOR("Texas Instruments Inc");
