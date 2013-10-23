/*
 * tmp103 Temperature sensor driver file
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

#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/reboot.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/i2c/tmp103.h>
#include <linux/thermal_framework.h>

#include <plat/common.h>

#include <plat/omap_device.h>
#include <plat/omap-pm.h>

#ifdef CONFIG_AMAZON_METRICS_LOG
#include <linux/metricslog.h>
#define TMP103_METRICS_STR_LEN 128
#endif

#define	TMP103_TEMP_REG		0x00
#define	TMP103_CONF_REG		0x01
#define	TMP103_TLOW_REG		0x02
#define	TMP103_THIGH_REG	0x03

#define	TMP103_MODE_MASK	(~0x03)
#define	TMP103_RATE_MASK	(~0x60)

#define	TMP103_SHUTDOWN_MODE	0x00
#define	TMP103_ONESHOT_MODE	0x01
#define	TMP103_CONT_MODE	0x02
#define	TMP103_0_25HZ_RATE	0x00
#define	TMP103_1HZ_RATE		0x20
#define	TMP103_4HZ_RATE		0x40
#define	TMP103_8HZ_RATE		0x80

#define	TMP103_MAX_TEMP		125000
#define	TMP103_MAX_RECOVERY_RETRY_COUNT	10
#define	TMP103_MAX_READ_RETRY_COUNT	10
/*
 * tmp103_temp_sensor structure
 * @iclient - I2c client pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for reading current temp
 * @therm_fw - thermal device
 * @saved_config - buffered configuration value
 * @last_update - time stamp in jiffies when the last temp was read
 * @temp - current sensor temp
 * @offset - offset value to compute hotsopt
 * @offset_cpu - offset value to compute hotsopt for CPU domain
 * @slope - slope value to compute hotsopt
 * @slope_cpu - slope value to compute hotsopt for CPU domain
 */
struct tmp103_temp_sensor {
	struct i2c_client *iclient;
	struct device *dev;
	struct mutex sensor_mutex;
	struct thermal_dev *therm_fw;
	u8 saved_config;
	unsigned long last_update;
	int temp;
	int offset;
	int offset_cpu;
	int slope;
	int slope_cpu;
	bool is_suspended;
};

static inline int tmp103_read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s: I2C read error\n", __func__);
		return ret;
	}

	*data = ret;

	return 0;
}

static inline int tmp103_write_reg(struct i2c_client *client,
				u8 reg, u16 val)
{
	return i2c_smbus_write_byte_data(client, reg, val);
}

static inline int tmp103_reg_to_mC(u8 val)
{
	/*Negative numbers*/
	if (val & 0x80) {
		val = ~val + 1;
		return -(val*1000);
	}
	return val*1000;
}

/* convert milliCelsius to 8-bit TMP103 register value */
static inline u8 tmp103_mC_to_reg(int val)
{
	return (val / 1000);
}

static int tmp103_handle_abnormal_temp(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct tmp103_temp_sensor *tmp103 = platform_get_drvdata(pdev);
	struct i2c_client *client = tmp103->iclient;
	u8 current_temp;
	int i;
#ifdef CONFIG_AMAZON_METRICS_LOG
	char *tmp103_metric_prefix = "pcbsensor:def";
	char buf[TMP103_METRICS_STR_LEN];

	/* Log in metrics */
	snprintf(buf,
		TMP103_METRICS_STR_LEN,
		"%s:abnormal_temp=%d;CT;1:NR",
		tmp103_metric_prefix, tmp103_mC_to_reg(TMP103_MAX_TEMP));
	log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
#endif
	pr_err("TMP103 reads temperature above %dC",
			tmp103_mC_to_reg(TMP103_MAX_TEMP));

	/* retry */
	mutex_lock(&tmp103->sensor_mutex);
	for (i = 0; i < TMP103_MAX_RECOVERY_RETRY_COUNT; i++) {
		int ret = 0;
		int count = 0;

		/* Switch to shutdown mode, then swtich back to previous mode. */
		tmp103_write_reg(client, TMP103_CONF_REG, TMP103_SHUTDOWN_MODE);
		msleep(50);
		tmp103_write_reg(client, TMP103_CONF_REG, tmp103->saved_config);
		msleep(50);

		/* Read the temperature. Retry in case of I2C host problems. */
		for (count = 0; count < TMP103_MAX_READ_RETRY_COUNT; count++) {
			ret = tmp103_read_reg(client, TMP103_TEMP_REG,
							&current_temp);
			if (ret >= 0)
				break;
			else
				msleep(10);
		}

		if (ret < 0) {
			pr_err("%s error reading I2C\n", __func__);
			mutex_unlock(&tmp103->sensor_mutex);
			return ret;
		}

		if (current_temp < tmp103_mC_to_reg(TMP103_MAX_TEMP))
			break;

#ifdef CONFIG_AMAZON_METRICS_LOG
		/* Log in metrics */
		snprintf(buf, TMP103_METRICS_STR_LEN,
			"%s:pcbtemp=%d;CT;1,retry=%d;CT;1:NR",
			tmp103_metric_prefix, current_temp, i);
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
#endif
		pr_info("TMP103 reads temperature %d, retry %d ",
				current_temp, i+1);
	}

	mutex_unlock(&tmp103->sensor_mutex);

	if (i >= TMP103_MAX_RECOVERY_RETRY_COUNT) {
		/* Sensor is unrecovarable, Shutdown the device to prevent */
		/* damage from overheating. */
#ifdef CONFIG_AMAZON_METRICS_LOG
		snprintf(buf, TMP103_METRICS_STR_LEN,
			"%s:saved_temp=%d;CT;1,use_saved_temp=1;CT;1:NR",
			tmp103_metric_prefix, current_temp);
		log_to_metrics(ANDROID_LOG_INFO, "ThermalEvent", buf);
#endif
		pr_emerg("FATAL: TMP103 locked down, return faulty temperature %d,"
				" system will shutdown....", current_temp);
		orderly_poweroff(true);
		/* Prevent consequent reads from TMP103 until shutdown.*/
		tmp103->is_suspended = true;
	}

	tmp103->temp = tmp103_reg_to_mC(current_temp);
	tmp103->last_update = jiffies;
	return tmp103->temp;
}

static int tmp103_read_current_temp(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp103_temp_sensor *tmp103 = i2c_get_clientdata(client);

	mutex_lock(&tmp103->sensor_mutex);
	if(tmp103->is_suspended){
		mutex_unlock(&tmp103->sensor_mutex);
		dev_dbg(&client->dev, "trying to read while suspended!\n");
		/* By design temp sensors are expected to return only positive
		temperature values, so negative ones are treated as error codes. */
		return -ECANCELED;
	}
	/* Prevent reading of the sensor more than once in 200ms. */
	if (time_after(jiffies, tmp103->last_update + HZ / 5)) {
		u8 val;
		int ret = tmp103_read_reg(client, TMP103_TEMP_REG, &val);
		if (ret < 0) {
			mutex_unlock(&tmp103->sensor_mutex);
			return ret;
		}
		tmp103->temp = tmp103_reg_to_mC(val);
		tmp103->last_update = jiffies;
	}
	mutex_unlock(&tmp103->sensor_mutex);

	return tmp103->temp;
}

static int tmp103_get_temp(struct thermal_dev *tdev)
{
	int temp = tmp103_read_current_temp(tdev->dev);

	if (unlikely(temp >= TMP103_MAX_TEMP))
		temp = tmp103_handle_abnormal_temp(tdev);

	return temp;
}

static int tmp103_report_slope(struct thermal_dev *tdev, const char *dom_name)
{
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp103_temp_sensor *tmp103 = i2c_get_clientdata(client);

	if (!strcmp(dom_name, "cpu"))
		return tmp103->slope;
	else
		return tmp103->slope_cpu;
}

static int tmp103_report_offset(struct thermal_dev *tdev, const char *dom_name)
{
	struct i2c_client *client = to_i2c_client(tdev->dev);
	struct tmp103_temp_sensor *tmp103 = i2c_get_clientdata(client);

	if (!strcmp(dom_name, "cpu"))
		return tmp103->offset;
	else
		return tmp103->offset_cpu;
}

static int tmp103_temp_sensor_read_temp(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int temp = tmp103_read_current_temp(dev);

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(temperature, S_IRUGO, tmp103_temp_sensor_read_temp,
			  NULL);

static struct attribute *tmp103_temp_sensor_attributes[] = {
	&dev_attr_temperature.attr,
	NULL
};

static const struct attribute_group tmp103_temp_sensor_attr_group = {
	.attrs = tmp103_temp_sensor_attributes,
};

static struct thermal_dev_ops tmp103_temp_sensor_ops = {
	.report_temp = tmp103_get_temp,
	.init_slope  = tmp103_report_slope,
	.init_offset = tmp103_report_offset
};

static int __devinit tmp103_temp_sensor_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tmp103_temp_sensor *tmp103;
	struct tmp103_platform_data *tmp103_platform_data;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "adapter doesn't support SMBus word transactions\n");
		return -ENODEV;
	}

	tmp103_platform_data = dev_get_platdata(&client->dev);
	if (!tmp103_platform_data) {
		dev_err(&client->dev, "%s: Invalid platform data\n", __func__);
		return -EINVAL;
	}

	tmp103 = kzalloc(sizeof(struct tmp103_temp_sensor), GFP_KERNEL);
	if (!tmp103) {
		dev_err(&client->dev, "%s: No Memory\n", __func__);
		return -ENOMEM;
	}

	mutex_init(&tmp103->sensor_mutex);

	tmp103->iclient = client;
	tmp103->dev = &client->dev;

	/*We want TMP103 to continuously measure temperature with 4HZ rate. */
	tmp103->saved_config = TMP103_4HZ_RATE | TMP103_CONT_MODE;

	ret = tmp103_write_reg(client, TMP103_CONF_REG, tmp103->saved_config);
	if (ret < 0) {
		dev_err(&client->dev, "error writing config register\n");
		goto free_err;
	}

	tmp103->last_update = jiffies - HZ;
	tmp103->offset = tmp103_platform_data->offset;
	tmp103->offset_cpu = tmp103_platform_data->offset_cpu;
	tmp103->slope = tmp103_platform_data->slope;
	tmp103->slope_cpu = tmp103_platform_data->slope_cpu;
	tmp103->is_suspended = false;

	tmp103->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (tmp103->therm_fw) {
		char tmp[30];

		snprintf(tmp, sizeof(tmp), "%s.%d", client->name,
			 client->addr);
		tmp103->therm_fw->name = kstrdup(tmp, GFP_KERNEL);
		tmp103->therm_fw->domain_name =
				kstrdup(tmp103_platform_data->domain,
					GFP_KERNEL);

		tmp103->therm_fw->dev = tmp103->dev;
		tmp103->therm_fw->dev_ops = &tmp103_temp_sensor_ops;
		ret = thermal_sensor_dev_register(tmp103->therm_fw);
		if (ret)
			goto therm_dev_reg_err;
	} else {
		ret = -ENOMEM;
		goto therm_fw_alloc_err;
	}

	ret = sysfs_create_group(&client->dev.kobj,
		&tmp103_temp_sensor_attr_group);

	if (ret)
		dev_err(&client->dev, "Cannot create attribures!\n");

	i2c_set_clientdata(client, tmp103);

	dev_info(&client->dev, "initialized\n");

	return 0;


therm_dev_reg_err:
	kfree(tmp103->therm_fw);
therm_fw_alloc_err:
free_err:
	mutex_destroy(&tmp103->sensor_mutex);
	kfree(tmp103);

	return ret;
}

static int __devexit tmp103_temp_sensor_remove(struct i2c_client *client)
{
	struct tmp103_temp_sensor *tmp103 = i2c_get_clientdata(client);

	thermal_sensor_dev_unregister(tmp103->therm_fw);

	tmp103_write_reg(client, TMP103_CONF_REG, TMP103_SHUTDOWN_MODE);

	kfree(tmp103->therm_fw->name);
	kfree(tmp103->therm_fw->domain);
	kfree(tmp103->therm_fw);
	kfree(tmp103);

	return 0;
}

#ifdef CONFIG_PM
static int tmp103_temp_sensor_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp103_temp_sensor *tmp103 = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&tmp103->sensor_mutex);

	ret = tmp103_write_reg(client, TMP103_CONF_REG, TMP103_SHUTDOWN_MODE);
	if (ret < 0)
		dev_err(&client->dev, "%s: tmp103 write error\n", __func__);
	else
		tmp103->is_suspended = true;

	mutex_unlock(&tmp103->sensor_mutex);
	return ret;
}

static int tmp103_temp_sensor_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp103_temp_sensor *tmp103 = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&tmp103->sensor_mutex);

	ret = tmp103_write_reg(client, TMP103_CONF_REG, tmp103->saved_config);
	if (ret < 0)
		dev_err(&client->dev, "%s: tmp103 write error\n", __func__);
	else
		tmp103->is_suspended = false;

	mutex_unlock(&tmp103->sensor_mutex);
	return ret;
}

static const struct dev_pm_ops tmp103_dev_pm_ops = {
	.suspend = tmp103_temp_sensor_suspend,
	.resume = tmp103_temp_sensor_resume,
};

#define TMP103_DEV_PM_OPS (&tmp103_dev_pm_ops)
#else
#define TMP103_DEV_PM_OPS NULL
#endif /* CONFIG_PM */

static void  tmp103_shutdown(struct i2c_client *client)
{
	struct tmp103_temp_sensor *tmp103 = i2c_get_clientdata(client);
	thermal_sensor_dev_unregister(tmp103->therm_fw);
}

static const struct i2c_device_id tmp103_id[] = {
	{ "tmp103_temp_sensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmp103_id);

static struct i2c_driver tmp103_driver = {
	.probe = tmp103_temp_sensor_probe,
	.remove = tmp103_temp_sensor_remove,
	.driver = {
		.name = "tmp103_temp_sensor",
		.pm = TMP103_DEV_PM_OPS,
	},
	.id_table = tmp103_id,
	.shutdown = tmp103_shutdown,
};

static int __init tmp103_init(void)
{
	return i2c_add_driver(&tmp103_driver);
}
module_init(tmp103_init);

static void __exit tmp103_exit(void)
{
	i2c_del_driver(&tmp103_driver);
}
module_exit(tmp103_exit);

MODULE_DESCRIPTION("TMP103 Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform: tmp103");
MODULE_AUTHOR("Texas Instruments Inc");
