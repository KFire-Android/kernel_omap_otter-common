/*
 * TMP102 Temperature sensor driver file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Steven King <sfking@fdwdc.com>
 * Author: Sabatier, Sebastien" <s-sabatier1@ti.com>
 * Author: Mandrenko, Ievgen" <ievgen.mandrenko@ti.com>
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
#include <linux/i2c/tmp102.h>
#include <linux/thermal_framework.h>

#include <plat/common.h>

#define REPORT_DELAY_MS 1000

#define TMP102_TEMP_REG 0x00
#define TMP102_CONF_REG 0x01
#define TMP102_TLOW_REG 0x02
#define TMP102_THIGH_REG 0x03
/* note: these bit definitions are byte swapped */
#define TMP102_CONF_SD 0x0100
#define TMP102_CONF_TM 0x0200
#define TMP102_CONF_POL 0x0400
#define TMP102_CONF_F0 0x0800
#define TMP102_CONF_F1 0x1000
#define TMP102_CONF_R0 0x2000
#define TMP102_CONF_R1 0x4000
#define TMP102_CONF_OS 0x8000
#define TMP102_CONF_EM 0x0010
#define TMP102_CONF_AL 0x0020
#define TMP102_CONF_CR0 0x0040
#define TMP102_CONF_CR1 0x0080
#define TMP102_TEMP_EM 0x01

#define TMP102_CONFIG  (TMP102_CONF_TM | TMP102_CONF_EM | TMP102_CONF_CR1)
#define TMP102_CONFIG_RD_ONLY (TMP102_CONF_R0 | TMP102_CONF_R1 | TMP102_CONF_AL)

/* According to tmp102 data sheet, the max value of temperature. */
#define TMP102_MAX_TEMP 128

/*
 * tmp102_temp_sensor structure
 * @iclient - I2c client pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for reading current temp
 * @therm_fw - thermal device
 * @config_orig - original configuration value
 * @last_update - time stamp in jiffies when the last temp was read
 * @temp - current sensor temp
 * @offset - offset value to compute hotsopt
 * @offset_cpu - offset value to compute hotsopt for CPU domain
 * @slope - slope value to compute hotsopt
 * @slope_cpu - slope value to compute hotsopt for CPU domain
 */
struct tmp102_temp_sensor {
	struct i2c_client *iclient;
	struct device *dev;
	struct mutex sensor_mutex;
	struct thermal_dev *therm_fw;
	u16 config_orig;
	unsigned long last_update;
	int temp[3];
	int offset;
	int offset_cpu;
	int slope;
	int slope_cpu;
};

static const u8 tmp102_reg[] = {
	TMP102_TEMP_REG,
	TMP102_TLOW_REG,
	TMP102_THIGH_REG,
};

/* SMBus specifies low byte first, but the TMP102 returns high byte first,
 * so we have to swab16 the values */
static inline int tmp102_read_reg(struct i2c_client *client, u8 reg, u16 *data)
{
	int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s: I2C read error\n", __func__);
		return ret;
	}

	*data = swab16(ret);

	return 0;
}

static inline int tmp102_write_reg(struct i2c_client *client,
				u8 reg, u16 val)
{
	return i2c_smbus_write_word_data(client, reg, swab16(val));
}

/* convert left adjusted 13-bit TMP102 register value to milliCelsius */
static inline int tmp102_reg_to_mc(s16 val)
{
	return ((val & ~TMP102_TEMP_EM) * 1000) / TMP102_MAX_TEMP;
}

/* convert milliCelsius to left adjusted 13-bit TMP102 register value */
static inline u16 tmp102_mc_to_reg(int val)
{
	return (val * TMP102_MAX_TEMP) / 1000;
}

static int tmp102_read_current_temp(struct device *dev)
{
	int index = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	mutex_lock(&tmp102->sensor_mutex);
	if (time_after(jiffies, tmp102->last_update + HZ / 3)) {
		u16 val;
		int ret = tmp102_read_reg(client, tmp102_reg[index], &val);
		if (ret < 0) {
			mutex_unlock(&tmp102->sensor_mutex);
			return ret;
		}
		tmp102->temp[index] = tmp102_reg_to_mc(val);
		tmp102->last_update = jiffies;
	}
	mutex_unlock(&tmp102->sensor_mutex);

	return tmp102->temp[index];
}

static int tmp102_get_temp(struct thermal_dev *tdev)
{
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	tmp102->therm_fw->current_temp =
			tmp102_read_current_temp(tdev->dev);

	return tmp102->therm_fw->current_temp;
}

static int tmp102_report_slope(struct thermal_dev *tdev, const char *dom_name)
{
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	if (!strcmp(dom_name, "cpu"))
		return tmp102->slope_cpu;
	else
		return tmp102->slope;
}

static int tmp102_report_offset(struct thermal_dev *tdev, const char *dom_name)
{
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	if (!strcmp(dom_name, "cpu"))
		return tmp102->offset_cpu;
	else
		return tmp102->offset;
}

static struct thermal_dev_ops tmp102_temp_sensor_ops = {
	.report_temp = tmp102_get_temp,
	.init_slope = tmp102_report_slope,
	.init_offset = tmp102_report_offset
};

static int __devinit tmp102_temp_sensor_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tmp102_temp_sensor *tmp102;
	struct tmp102_platform_data *tmp102_platform_data;
	int ret = 0;
	u16 val;

	if (!i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "adapter doesn't support SMBus word transactions\n");
		return -ENODEV;
	}

	tmp102_platform_data = dev_get_platdata(&client->dev);
	if (!tmp102_platform_data) {
		dev_err(&client->dev, "%s: Invalid platform data\n", __func__);
		return -EINVAL;
	}

	tmp102 = kzalloc(sizeof(struct tmp102_temp_sensor), GFP_KERNEL);
	if (!tmp102) {
		dev_err(&client->dev, "%s: No Memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&tmp102->sensor_mutex);

	tmp102->iclient = client;
	tmp102->dev = &client->dev;

	ret = tmp102_read_reg(client, TMP102_CONF_REG, &val);
	if (ret < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto free_err;
	}
	tmp102->config_orig = val;

	ret = tmp102_write_reg(client, TMP102_CONF_REG, TMP102_CONFIG);
	if (ret < 0) {
		dev_err(&client->dev, "error writing config register %d\n",
				ret);
		goto restore_config_err;
	}

	ret = tmp102_read_reg(client, TMP102_CONF_REG, &val);
	if (ret < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto restore_config_err;
	}

	val &= ~TMP102_CONFIG_RD_ONLY;
	if (val != TMP102_CONFIG) {
		dev_err(&client->dev, "config settings did not stick\n");
		ret = -ENODEV;
		goto restore_config_err;
	}

	tmp102->last_update = jiffies - HZ;
	tmp102->offset = tmp102_platform_data->offset;
	tmp102->offset_cpu = tmp102_platform_data->offset_cpu;
	tmp102->slope = tmp102_platform_data->slope;
	tmp102->slope_cpu = tmp102_platform_data->slope_cpu;

	tmp102->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (tmp102->therm_fw) {
		char tmp[30];

		snprintf(tmp, sizeof(tmp), "%s.%d", client->name,
			 client->addr);
		tmp102->therm_fw->name = kstrdup(tmp, GFP_KERNEL);
		tmp102->therm_fw->domain_name =
				kstrdup(tmp102_platform_data->domain,
					GFP_KERNEL);

		tmp102->therm_fw->dev = tmp102->dev;
		tmp102->therm_fw->dev_ops = &tmp102_temp_sensor_ops;
		ret = thermal_sensor_dev_register(tmp102->therm_fw);
		if (ret)
			goto therm_dev_reg_err;
	} else {
		ret = -ENOMEM;
		goto therm_fw_alloc_err;
	}

	i2c_set_clientdata(client, tmp102);

	dev_info(&client->dev, "initialized\n");

	return 0;


therm_dev_reg_err:
	kfree(tmp102->therm_fw);
therm_fw_alloc_err:
restore_config_err:
	tmp102_write_reg(client, TMP102_CONF_REG, tmp102->config_orig);
free_err:
	mutex_destroy(&tmp102->sensor_mutex);
	kfree(tmp102);

	return ret;
}

static int __devexit tmp102_temp_sensor_remove(struct i2c_client *client)
{
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);
	u16 config;

	thermal_sensor_dev_unregister(tmp102->therm_fw);

	/* Stop monitoring if device was stopped originally */
	if (tmp102->config_orig & TMP102_CONF_SD) {
		int ret;

		ret = tmp102_read_reg(client, TMP102_CONF_REG, &config);
		if (ret < 0)
			/* Even if the read fails, need to free the memory */
			goto free_drv;
		tmp102_write_reg(client, TMP102_CONF_REG,
					config | TMP102_CONF_SD);
	}

free_drv:
	kfree(tmp102->therm_fw->name);
	kfree(tmp102->therm_fw->domain);
	kfree(tmp102->therm_fw);
	kfree(tmp102);

	return 0;
}

#ifdef CONFIG_PM
static int tmp102_temp_sensor_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u16 config;

	ret = tmp102_read_reg(client, TMP102_CONF_REG, &config);
	if (ret < 0) {
		dev_err(&client->dev, "%s: tmp102 read error\n", __func__);
		return ret;
	}

	config |= TMP102_CONF_SD;
	ret = tmp102_write_reg(client, TMP102_CONF_REG, config);
	if (ret < 0)
		dev_err(&client->dev, "%s: tmp102 write error\n", __func__);

	return ret;
}

static int tmp102_temp_sensor_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;
	u16 config;

	ret = tmp102_read_reg(client, TMP102_CONF_REG, &config);
	if (ret < 0) {
		dev_err(&client->dev, "%s: tmp102 read error\n", __func__);
		return ret;
	}

	config &= ~TMP102_CONF_SD;
	ret = tmp102_write_reg(client, TMP102_CONF_REG, config);
	if (ret < 0)
		dev_err(&client->dev, "%s: tmp102 write error\n", __func__);

	return ret;
}
static const struct dev_pm_ops tmp102_dev_pm_ops = {
	.suspend = tmp102_temp_sensor_suspend,
	.resume = tmp102_temp_sensor_resume,
};

#define TMP102_DEV_PM_OPS (&tmp102_dev_pm_ops)
#else
#define TMP102_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id tmp102_id[] = {
	{ "tmp102_temp_sensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

static struct i2c_driver tmp102_driver = {
	.probe = tmp102_temp_sensor_probe,
	.remove = tmp102_temp_sensor_remove,
	.driver = {
		.name = "tmp102_temp_sensor",
		.pm = TMP102_DEV_PM_OPS,
	},
	.id_table = tmp102_id,
};

static int __init tmp102_init(void)
{
	return i2c_add_driver(&tmp102_driver);
}
module_init(tmp102_init);

static void __exit tmp102_exit(void)
{
	i2c_del_driver(&tmp102_driver);
}
module_exit(tmp102_exit);

MODULE_DESCRIPTION("TMP102 Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: tmp102");
MODULE_AUTHOR("Texas Instruments Inc");
