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
#include <plat/common.h>
#include <plat/tmp102_temp_sensor.h>

#include <plat/omap_device.h>
#include <plat/omap-pm.h>

#include <linux/thermal_framework.h>

#define REPORT_DELAY_MS	1000

#define	TMP102_TEMP_REG			0x00
#define	TMP102_CONF_REG			0x01
/* note: these bit definitions are byte swapped */
#define		TMP102_CONF_SD		0x0100
#define		TMP102_CONF_TM		0x0200
#define		TMP102_CONF_POL		0x0400
#define		TMP102_CONF_F0		0x0800
#define		TMP102_CONF_F1		0x1000
#define		TMP102_CONF_R0		0x2000
#define		TMP102_CONF_R1		0x4000
#define		TMP102_CONF_OS		0x8000
#define		TMP102_CONF_EM		0x0010
#define		TMP102_CONF_AL		0x0020
#define		TMP102_CONF_CR0		0x0040
#define		TMP102_CONF_CR1		0x0080
#define		TMP102_TLOW_REG		0x02
#define		TMP102_THIGH_REG	0x03

/* Addresses scanned */
static const unsigned short normal_i2c[] = { 0x48, 0x49, 0x4a, 0x4b, 0x4c,
				0x4d, 0x4e, 0x4f, I2C_CLIENT_END };

/*
 * omap_temp_sensor structure
 * @iclient - I2c client pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for sysfs, irq and PM
 * @therm_fw - thermal device
 */
struct tmp102_temp_sensor {
	struct i2c_client *iclient;
	struct device *dev;
	struct mutex sensor_mutex;
	struct thermal_dev *therm_fw;
	u16 config_orig;
	unsigned long last_update;
	int temp[3];
	struct delayed_work tmp102_sensor_work;
	int work_delay;
	int debug_temp;
};

/* SMBus specifies low byte first, but the TMP102 returns high byte first,
 * so we have to swab16 the values */
static inline int tmp102_read_reg(struct i2c_client *client, u8 reg)
{
	int result = i2c_smbus_read_word_data(client, reg);

	return result < 0 ? result : swab16(result);
}

static inline int tmp102_write_reg(struct i2c_client *client,
				u8 reg, u16 val)
{
	return i2c_smbus_write_word_data(client, reg, swab16(val));
}

/* convert left adjusted 13-bit TMP102 register value to milliCelsius */
static inline int tmp102_reg_to_mC(s16 val)
{
	return ((val & ~0x01) * 1000) / 128;
}

/* convert milliCelsius to left adjusted 13-bit TMP102 register value */
static inline u16 tmp102_mC_to_reg(int val)
{
	return (val * 128) / 1000;
}

static const u8 tmp102_reg[] = {
	TMP102_TEMP_REG,
	TMP102_TLOW_REG,
	TMP102_THIGH_REG,
};

static int tmp102_read_current_temp(struct device *dev)
{
	int index = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	tmp102 = i2c_get_clientdata(client);

	mutex_lock(&tmp102->sensor_mutex);
	if (time_after(jiffies, tmp102->last_update + HZ / 3)) {
		int status = tmp102_read_reg(client, tmp102_reg[index]);
		if (status > -1)
			tmp102->temp[index] = tmp102_reg_to_mC(status);
		tmp102->last_update = jiffies;
	}
	mutex_unlock(&tmp102->sensor_mutex);

	return tmp102->temp[index];
}

static int tmp102_get_temp(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct tmp102_temp_sensor *tmp102 = platform_get_drvdata(pdev);

	tmp102->therm_fw->current_temp =
			tmp102_read_current_temp(tdev->dev);

	return tmp102->therm_fw->current_temp;
}

static void tmp102_report_temp(struct tmp102_temp_sensor *tmp102)
{
	int ret;

	tmp102->therm_fw->current_temp = tmp102_read_current_temp(tmp102->dev);

	if (tmp102->therm_fw->current_temp != -EINVAL) {
		ret = thermal_sensor_set_temp(tmp102->therm_fw);
		if (ret == -ENODEV)
			pr_err("%s:thermal_sensor_set_temp reports error\n",
				__func__);
		else
			cancel_delayed_work_sync(&tmp102->tmp102_sensor_work);
		kobject_uevent(&tmp102->dev->kobj, KOBJ_CHANGE);
	}
}

/*
 * sysfs hook functions
 */
static ssize_t tmp102_show_temp_user_space(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", tmp102->debug_temp);
}

static ssize_t tmp102_set_temp_user_space(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);
	long val;

	if (strict_strtol(buf, 10, &val)) {
		count = -EINVAL;
		goto out;
	}

	/* Set new temperature */
	tmp102->debug_temp = val;

	tmp102->therm_fw->current_temp = val;
	thermal_sensor_set_temp(tmp102->therm_fw);
	/* Send a kobj_change */
	kobject_uevent(&tmp102->dev->kobj, KOBJ_CHANGE);

out:
	return count;
}

static int tmp102_temp_sensor_read_temp(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int temp = tmp102_read_current_temp(dev);

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(debug_user, S_IWUSR | S_IRUGO, tmp102_show_temp_user_space,
			  tmp102_set_temp_user_space);
static DEVICE_ATTR(temp1_input, S_IRUGO, tmp102_temp_sensor_read_temp,
			  NULL);

static struct attribute *tmp102_temp_sensor_attributes[] = {
	&dev_attr_temp1_input.attr,
	&dev_attr_debug_user.attr,
	NULL
};

static const struct attribute_group tmp102_temp_sensor_attr_group = {
	.attrs = tmp102_temp_sensor_attributes,
};

static struct thermal_dev_ops tmp102_temp_sensor_ops = {
	.report_temp = tmp102_get_temp,
};

static void tmp102_sensor_delayed_work_fn(struct work_struct *work)
{
	struct tmp102_temp_sensor *tmp102 =
			container_of(work, struct tmp102_temp_sensor,
					tmp102_sensor_work.work);

	tmp102_report_temp(tmp102);

	schedule_delayed_work(&tmp102->tmp102_sensor_work,
				msecs_to_jiffies(tmp102->work_delay));
}

#define TMP102_CONFIG  (TMP102_CONF_TM | TMP102_CONF_EM | TMP102_CONF_CR1)
#define TMP102_CONFIG_RD_ONLY (TMP102_CONF_R0 | TMP102_CONF_R1 | TMP102_CONF_AL)

static int __devinit tmp102_temp_sensor_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tmp102_temp_sensor *tmp102;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "adapter doesn't support SMBus word "
			"transactions\n");

		return -ENODEV;
	}

	tmp102 = kzalloc(sizeof(struct tmp102_temp_sensor), GFP_KERNEL);
	if (!tmp102)
		return -ENOMEM;

	/* Init delayed work for PCB sensor temperature */
	INIT_DELAYED_WORK(&tmp102->tmp102_sensor_work,
			  tmp102_sensor_delayed_work_fn);

	mutex_init(&tmp102->sensor_mutex);

	tmp102->iclient = client;
	tmp102->dev = &client->dev;

	kobject_uevent(&client->dev.kobj, KOBJ_ADD);
	i2c_set_clientdata(client, tmp102);

	ret = tmp102_read_reg(client, TMP102_CONF_REG);
	if (ret < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto free_err;
	}
	tmp102->config_orig = ret;
	ret = tmp102_write_reg(client, TMP102_CONF_REG, TMP102_CONFIG);
	if (ret < 0) {
		dev_err(&client->dev, "error writing config register\n");
		goto restore_config_err;
	}
	ret = tmp102_read_reg(client, TMP102_CONF_REG);
	if (ret < 0) {
		dev_err(&client->dev, "error reading config register\n");
		goto restore_config_err;
	}
	ret &= ~TMP102_CONFIG_RD_ONLY;
	if (ret != TMP102_CONFIG) {
		dev_err(&client->dev, "config settings did not stick\n");
		ret = -ENODEV;
		goto restore_config_err;
	}
	tmp102->last_update = jiffies - HZ;
	mutex_init(&tmp102->sensor_mutex);

	ret = sysfs_create_group(&client->dev.kobj,
		&tmp102_temp_sensor_attr_group);
	if (ret)
		goto sysfs_create_err;

	tmp102->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (tmp102->therm_fw) {
		tmp102->therm_fw->name = TMP102_SENSOR_NAME;
		tmp102->therm_fw->domain_name = "cpu";
		tmp102->therm_fw->dev = tmp102->dev;
		tmp102->therm_fw->dev_ops = &tmp102_temp_sensor_ops;
		thermal_sensor_dev_register(tmp102->therm_fw);
	} else {
		ret = -ENOMEM;
		goto therm_fw_alloc_err;
	}

	tmp102->work_delay = REPORT_DELAY_MS;
	schedule_delayed_work(&tmp102->tmp102_sensor_work,
			msecs_to_jiffies(0));

	dev_info(&client->dev, "initialized\n");

	return 0;

sysfs_create_err:
	thermal_sensor_dev_unregister(tmp102->therm_fw);
	kfree(tmp102->therm_fw);
restore_config_err:
	tmp102_write_reg(client, TMP102_CONF_REG, tmp102->config_orig);
therm_fw_alloc_err:
free_err:
	mutex_destroy(&tmp102->sensor_mutex);
	kfree(tmp102);

	return ret;
}

static int __devexit tmp102_temp_sensor_remove(struct i2c_client *client)
{
	struct tmp102_temp_sensor *tmp102 = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &tmp102_temp_sensor_attr_group);

	/* Stop monitoring if device was stopped originally */
	if (tmp102->config_orig & TMP102_CONF_SD) {
		int config;

		config = tmp102_read_reg(client, TMP102_CONF_REG);
		if (config >= 0)
			tmp102_write_reg(client, TMP102_CONF_REG,
					 config | TMP102_CONF_SD);
	}

	kfree(tmp102);

	return 0;
}

#ifdef CONFIG_PM
static int tmp102_temp_sensor_suspend(struct i2c_client *client,
			pm_message_t mesg)
{
	int conf = tmp102_read_reg(client, TMP102_CONF_REG);

	if (conf < 0)
		return conf;
	conf |= TMP102_CONF_SD;

	return tmp102_write_reg(client, TMP102_CONF_REG, conf);
}

static int tmp102_temp_sensor_resume(struct i2c_client *client)
{
	int conf = tmp102_read_reg(client, TMP102_CONF_REG);

	if (conf < 0)
		return conf;
	conf &= ~TMP102_CONF_SD;

	return tmp102_write_reg(client, TMP102_CONF_REG, conf);
}

#else

tmp102_temp_sensor_suspend NULL
tmp102_temp_sensor_resume NULL

#endif /* CONFIG_PM */

static int tmp102_detect(struct i2c_client *new_client,
		       struct i2c_board_info *info)
{
	strlcpy(info->type, "tmp102_temp_sensor", I2C_NAME_SIZE);

	return 0;
}

static const struct i2c_device_id tmp102_id[] = {
	{ "tmp102_temp_sensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

static struct i2c_driver tmp102_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe = tmp102_temp_sensor_probe,
	.remove = tmp102_temp_sensor_remove,
	.suspend = tmp102_temp_sensor_suspend,
	.resume = tmp102_temp_sensor_resume,
	.driver = {
		.name = "tmp102_temp_sensor",
	},
	.id_table	= tmp102_id,
	.detect		= tmp102_detect,
	.address_list	= normal_i2c,
};

static int __init tmp102_init(void)
{
	if (!cpu_is_omap447x())
		return 0;

	return i2c_add_driver(&tmp102_driver);
}
module_init(tmp102_init);

static void __exit tmp102_exit(void)
{
	i2c_del_driver(&tmp102_driver);
}
module_exit(tmp102_exit);

MODULE_DESCRIPTION("OMAP447X TMP102 Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");
