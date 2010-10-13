/*
 * bh1780gli.c
 * ROHM Ambient Light Sensor Driver
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Hemanth V <hemanthv@ti.com>
 * Contributor: Dan Murphy <dmurphy@ti.com>
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define BH1780_REG_CONTROL     0x80
#define BH1780_REG_PARTID      0x8A
#define BH1780_REG_MANFID      0x8B
#define BH1780_REG_DLOW        0x8C
#define BH1780_REG_DHIGH       0x8D

#define BH1780_REVMASK        (0xf)
#define BH1780_POWMASK        (0x3)
#define BH1780_POFF           (0x0)
#define BH1780_PON            (0x3)

/* power on settling time in ms */
#define BH1780_PON_DELAY      2

/* Delayed work queue polling rate in seconds */
#define BH1780_POLL_RATE      5000

#define LUX_MAX_CHANGE	200

static uint32_t als_debug;
module_param_named(bh1780_debug, als_debug, uint, 0664);

struct bh1780_data {
	struct input_dev *input_dev;
	struct delayed_work input_work;
	struct i2c_client *client;
	struct mutex lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
       struct early_suspend early_suspend;
#endif
	int current_lux;
	int old_lux;
	int power_state;
	int suspend_power_state;
	int req_poll_rate;
};

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
static void bh1780gli_early_suspend(struct early_suspend *handler);
static void bh1780gli_late_resume(struct early_suspend *handler);
#endif
#endif

static int bh1780_write(struct bh1780_data *ddata, u8 reg, u8 val, char *msg)
{
	int ret = i2c_smbus_write_byte_data(ddata->client, reg, val);
	if (ret < 0)
		dev_err(&ddata->client->dev,
			"i2c_smbus_write_byte_data failed error %d\
			Register (%s)\n", ret, msg);
	return ret;
}

static int bh1780_read(struct bh1780_data *ddata, u8 reg, char *msg)
{
	int ret = i2c_smbus_read_byte_data(ddata->client, reg);
	if (ret < 0)
		dev_err(&ddata->client->dev,
			"i2c_smbus_read_byte_data failed error %d\
			 Register (%s)\n", ret, msg);
	return ret;
}

static int bh1780_read_data(struct bh1780_data *ddata)
{
	int lsb, msb;

	lsb = bh1780_read(ddata, BH1780_REG_DLOW, "DLOW");
	if (lsb < 0)
		return lsb;

	msb = bh1780_read(ddata, BH1780_REG_DHIGH, "DHIGH");
	if (msb < 0)
		return msb;

	if (als_debug)
		pr_info("%s:msb = 0x%X, lsb = 0x%X\n", __func__, msb, lsb);

	ddata->current_lux = (msb << 8) | lsb;

	if (als_debug)
		pr_info("%s:current lux = 0x%X\n",
			__func__, ddata->current_lux);

	return ddata->current_lux;
}

static void bh1780_report_data(struct bh1780_data *ddata)
{
	input_event(ddata->input_dev, EV_LED, LED_MISC, ddata->current_lux);
	input_sync(ddata->input_dev);
}

static int bh1780_set_power(struct bh1780_data *ddata, u8 val)
{
	int error = 0;

	mutex_lock(&ddata->lock);

	error = bh1780_write(ddata, BH1780_REG_CONTROL, val, "CONTROL");
	if (error < 0) {
		mutex_unlock(&ddata->lock);
		return error;
	}

	msleep(BH1780_PON_DELAY);
	ddata->power_state = val;
	mutex_unlock(&ddata->lock);

	if (val == BH1780_POFF)
		cancel_delayed_work_sync(&ddata->input_work);
	else
		schedule_delayed_work(&ddata->input_work, 0);

	return error;
}

static ssize_t bh1780_show_lux(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bh1780_data *ddata = platform_get_drvdata(pdev);
	int error = -1;

	if (ddata->power_state == BH1780_POFF)
		return error;

	error = bh1780_read_data(ddata);
	if (error < 0)
		return error;

	return sprintf(buf, "%d\n", ddata->current_lux);
}

static ssize_t bh1780_show_power_state(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bh1780_data *ddata = platform_get_drvdata(pdev);
	int state;

	state = bh1780_read(ddata, BH1780_REG_CONTROL, "CONTROL");
	if (state < 0)
		return state;

	return sprintf(buf, "%d\n", state & BH1780_POWMASK);
}

static ssize_t bh1780_store_power_state(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct bh1780_data *ddata = platform_get_drvdata(pdev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	if (val != BH1780_POFF)
		val = BH1780_PON;

	error = bh1780_set_power(ddata, val);
	if (error < 0)
		return error;

	return count;
}

static DEVICE_ATTR(lux, S_IRUGO, bh1780_show_lux, NULL);

static DEVICE_ATTR(power_state, S_IWUSR | S_IRUGO,
		bh1780_show_power_state, bh1780_store_power_state);

static struct attribute *bh1780_attributes[] = {
	&dev_attr_power_state.attr,
	&dev_attr_lux.attr,
	NULL
};

static const struct attribute_group bh1780_attr_group = {
	.attrs = bh1780_attributes,
};

static void bh1780_input_work_func(struct work_struct *work)
{
	struct bh1780_data *als = container_of((struct delayed_work *)work,
						  struct bh1780_data,
						  input_work);

	bh1780_read_data(als);

	if (abs(als->current_lux - als->old_lux) > LUX_MAX_CHANGE) {
		bh1780_report_data(als);
		als->old_lux = als->current_lux;
	}

	schedule_delayed_work(&als->input_work,
				msecs_to_jiffies(als->req_poll_rate));
}

static int __devinit bh1780_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int ret;
	struct bh1780_data *ddata = NULL;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		ret = -EIO;
		goto err_op_failed;
	}

	ddata = kzalloc(sizeof(struct bh1780_data), GFP_KERNEL);
	if (ddata == NULL) {
		ret = -ENOMEM;
		goto err_op_failed;
	}

	ddata->client = client;
	i2c_set_clientdata(client, ddata);

	ret = bh1780_read(ddata, BH1780_REG_PARTID, "PART ID");
	if (ret < 0)
		goto err_op_failed;

	dev_info(&client->dev, "Ambient Light Sensor, Rev : %d\n",
			(ret & BH1780_REVMASK));

	ddata->req_poll_rate = BH1780_POLL_RATE;
	ddata->old_lux = 0;
	ddata->current_lux = 0;
	INIT_DELAYED_WORK(&ddata->input_work, bh1780_input_work_func);

	ddata->input_dev = input_allocate_device();
	if (!ddata->input_dev) {
		ret = -ENOMEM;
		pr_err("%s: input device allocate failed: %d\n", __func__,
		       ret);
		goto error_input_allocate_failed;
	}

	ddata->input_dev->name = "bh1780gli";
	input_set_capability(ddata->input_dev, EV_LED, LED_MISC);

	ret = input_register_device(ddata->input_dev);
	if (ret) {
		pr_err("%s: input device register failed:%d\n", __func__,
		       ret);
		goto error_input_register_failed;
	}

	mutex_init(&ddata->lock);

	ret = sysfs_create_group(&client->dev.kobj, &bh1780_attr_group);
	if (ret)
		goto err_sysfs_failed;

	bh1780_set_power(ddata, BH1780_PON);
#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
       ddata->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
       ddata->early_suspend.suspend = bh1780gli_early_suspend;
       ddata->early_suspend.resume = bh1780gli_late_resume;
       register_early_suspend(&ddata->early_suspend);
#endif
#endif
	return 0;

err_sysfs_failed:
	input_unregister_device(ddata->input_dev);
error_input_register_failed:
	input_free_device(ddata->input_dev);
error_input_allocate_failed:
err_op_failed:
	kfree(ddata);
	return ret;
}

static int __devexit bh1780_remove(struct i2c_client *client)
{
	struct bh1780_data *ddata;

	ddata = i2c_get_clientdata(client);
	input_unregister_device(ddata->input_dev);
	sysfs_remove_group(&client->dev.kobj, &bh1780_attr_group);
	i2c_set_clientdata(client, NULL);
	kfree(ddata);

	return 0;
}

#ifdef CONFIG_PM
static int bh1780_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct bh1780_data *ddata;
	int ret;

	ddata = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&ddata->input_work);

	ddata->suspend_power_state = ddata->power_state;
	ret = bh1780_write(ddata, BH1780_REG_CONTROL, BH1780_POFF, "CONTROL");
	if (ret < 0)
		return ret;

	ddata->power_state = BH1780_POFF;
	return 0;
}

static int bh1780_resume(struct i2c_client *client)
{
	struct bh1780_data *ddata;
	int ret;

	ddata = i2c_get_clientdata(client);

	if (ddata->suspend_power_state == BH1780_PON) {
		if (als_debug)
			pr_info("%s:Setting als state to %i\n",
				__func__, ddata->suspend_power_state);

		ddata->power_state = ddata->suspend_power_state;
		ret = bh1780_write(ddata, BH1780_REG_CONTROL, ddata->power_state, "CONTROL");
		if (ret < 0)
			return ret;

		schedule_delayed_work(&ddata->input_work, 0);
	}

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bh1780gli_early_suspend(struct early_suspend *handler)
{
	struct bh1780_data *ddata;

	ddata = container_of(handler, struct bh1780_data, early_suspend);
	bh1780_suspend(ddata->client, PMSG_SUSPEND);
}

static void bh1780gli_late_resume(struct early_suspend *handler)
{
	struct bh1780_data *ddata;

	ddata = container_of(handler, struct bh1780_data, early_suspend);
	bh1780_resume(ddata->client);
}
#endif

#else
#define bh1780_suspend NULL
#define bh1780_resume NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id bh1780_id[] = {
	{ "bh1780", 0 },
	{ },
};

static struct i2c_driver bh1780_driver = {
	.probe		= bh1780_probe,
	.remove		= bh1780_remove,
	.id_table	= bh1780_id,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= bh1780_suspend,
	.resume		= bh1780_resume,
#endif
	.driver = {
		.name = "bh1780"
	},
};

static int __init bh1780_init(void)
{
	return i2c_add_driver(&bh1780_driver);
}

static void __exit bh1780_exit(void)
{
	i2c_del_driver(&bh1780_driver);
}

module_init(bh1780_init)
module_exit(bh1780_exit)

MODULE_DESCRIPTION("BH1780GLI Ambient Light Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hemanth V <hemanthv@ti.com>");
