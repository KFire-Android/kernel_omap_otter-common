/*
 *  stk_als22x7.c - Linux kernel modules for ambient light sensor
 *
 *  Copyright (C) 2011 Patrick Chang / SenseTek <patrick_chang@sitronix.com.tw>
 *                2013 Stephan Kanthak <stylon@gmx.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  History:
 *
 *  1.0    2010/08/15  Patrick Chang
 *         initial release
 *  1.2    2010/10/13  Patrick Chang
 *         added G-Sensor driver support
 *  1.3    2010/12/01  Patrick Chang
 *         added both polling mode and interrupt mode support
 *         added "transmittance" for lux-reading calibration
 *  1.4    2011/01/15  Patrick Chang
 *         added STK2207 support
 *  1.5    2011/03/05  Patrick Chang
 *         move driver path to /drivers/i2c/SenseTek
 *         change driver mode from devfs to sysfs and added input event support
 *         added STK31xx proximity sensors support
 *         added several tunable parameters in ENG/DBG mode (mount on /sys/devices/platform/stk-xxxx/DBG)
 *  1.51   2011/03/18  Patrick Chang
 *         modify all ALS drivers to support input event system
 *         temove R-set setting (use "transmittance" setting to cover)
 *  1.5.8  Patrick Chang
 *         original driver from SenseTek/Sitronix released with Kindle Fire source code
 *  2.0.0  2013/02/19  Stephan Kanthak
 *         revised driver to support suspend/resume
 *         moved everything to a single file
 *         removed all global variables
 *         cleaned up 1.5.8 driver, e.g. removed all debug code, binary sysfs stuff
 *         made hard-coded parameters configurable via sysfs
 *
 *  Info:
 *
 *  SenseTek/Sitronix module ST22x7 uses a CM3263 chip => no documentation available on the web
 */

#include <linux/completion.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>

#define DRIVER_NAME               "stk_als22x7"
#define DRIVER_VERSION            "2.0.0"
#define DEVICE_NAME               "lightsensor-level"

#define DEFAULT_SAMPLING_RATE     CONFIG_STK_ALS_SAMPLING_RATE
#define MAX_SAMPLING_RATE         4
#define DEFAULT_TRANSMITTANCE     CONFIG_STK_ALS_TRANSMITTANCE
#define DEFAULT_CHANGE_THRESHOLD  CONFIG_STK_ALS_CHANGE_THRESHOLD
#define ABS(x)                    ((x) >= 0 ? (x) : (-x))
#define MAX_REPORT                8192

#define ERR(format, args...)      printk(KERN_ERR "%s: " format, DRIVER_NAME, ## args)
#define WARNING(format, args...)  printk(KERN_WARNING "%s: " format, DRIVER_NAME, ## args)
#ifdef CONFIG_STK_SHOW_INFO
#define INFO(format, args...)     printk(KERN_INFO "%s: " format, DRIVER_NAME, ## args)
#else
#define INFO(format,args...)
#endif

#define STK_ALS_CMD1_GAIN(x)      ((x) << 6)
#define STK_ALS_CMD1_IT(x)        ((x) << 2)
#define STK_ALS_CMD1_WM(x)        ((x) << 1)
#define STK_ALS_CMD1_SD(x)        ((x) << 0)
#define STK_ALS_CMD2_FDIT(x)      ((x) << 5)

#define STK_ALS_CMD1_GAIN_MASK    0xc0
#define STK_ALS_CMD1_IT_MASK      0x0c
#define STK_ALS_CMD1_WM_MASK      0x02
#define STK_ALS_CMD1_SD_MASK      0x01
#define STK_ALS_CMD2_FDIT_MASK    0x0e

struct stk_als22x7_data {
    struct i2c_client *client1;
    struct i2c_client *client2;
    struct input_dev *input_dev;

    uint32_t sampling_rate;
    int32_t transmittance;
    int32_t change_threshold;

    uint32_t gain;
    uint32_t it;
    uint8_t reg1, reg2;
    int32_t lux_last;

    unsigned int enabled : 1;
    struct mutex lock;
    struct task_struct *thread;
    struct completion thread_completion;

#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif
};

static struct platform_device *stk_als22x7_device = NULL;
static struct stk_als22x7_data *stk_als22x7_device_data = NULL;

static int32_t set_it(struct stk_als22x7_data *data, uint32_t it) {
    data->it = it;
    data->reg1 &= (~STK_ALS_CMD1_IT_MASK);
    data->reg1 |= STK_ALS_CMD1_IT(it);
    return i2c_smbus_write_byte(data->client1, data->reg1);
}

static int32_t set_gain(struct stk_als22x7_data *data, uint32_t gain) {
    data->gain = gain;
    data->reg1 &= (~STK_ALS_CMD1_GAIN_MASK);
    data->reg1 |= STK_ALS_CMD1_GAIN(gain);
    return i2c_smbus_write_byte(data->client1, data->reg1);
}

static uint32_t stk_als22x7_init_hardware(struct stk_als22x7_data *data) {
    int err;
    set_it(data, 2); //x2
    set_gain(data, 1); //x2
    err = i2c_smbus_write_byte(data->client2, 64);
    INFO("i2c write status after init(): %d\n", err);
    return true;
}

static int32_t stk_als22x7_set_power_state(struct stk_als22x7_data *data, unsigned int shutdown) {
    data->reg1 &= (~STK_ALS_CMD1_SD_MASK);
    data->reg1 |= STK_ALS_CMD1_SD(shutdown);
    if (shutdown == 0)
	mdelay(500);
    return i2c_smbus_write_byte(data->client1, data->reg1);
}

inline int32_t stk_als22x7_code2lux(struct stk_als22x7_data *data, uint32_t alscode) {
    alscode = (alscode << 9) + (alscode << 6);
    // x 576
    // gain + it --> x 10.64 ==> total ~ x 6000
    // Org : 1 code = 0.6 lux
    // ==> 6000 code = 0.6 lux  (--> Transmittance Factor = 10,000 if directly incident)
    alscode /= data->transmittance;
    return alscode;
}

#if 0
inline uint32_t stk_als22x7_lux2code(struct stk_als22x7_data *data, uint32_t lux) {
    lux *= data->transmittance;
    lux /= 576;
    if (unlikely(lux >= (1 << 16)))
        lux = (1 << 16) - 1;
    return lux;
}
#endif

static int32_t stk_als22x7_get_lux(struct stk_als22x7_data *data) {
    int32_t raw;
    i2c_smbus_write_byte(data->client2, (uint8_t)(data->lux_last & 0x0f) | data->reg2);
    raw = (i2c_smbus_read_byte(data->client1) << 8) | i2c_smbus_read_byte(data->client2);
    // 1 code = 0.6 lux (for 100ms GAIN = 00)
    // x10.64
    return stk_als22x7_code2lux(data, raw);
}

inline void stk_als22x7_report(struct stk_als22x7_data *data, int32_t lux) {
    int32_t lux_last = data->lux_last;
    // this fixes an oddly high start report when sensor is first enabled
    if (lux > MAX_REPORT)
        lux = 0;
    if (unlikely(abs(lux - lux_last) >= data->change_threshold)) {
        data->lux_last = lux;
	input_report_abs(data->input_dev, ABS_MISC, lux);
	input_sync(data->input_dev);
	INFO("input event %d lux\n", lux);
    }
}

static int stk_als22x7_poll(void* arg) {
    uint32_t lux, delay;
    struct stk_als22x7_data *data = (struct stk_als22x7_data*)arg;

    init_completion(&data->thread_completion);
    while (1) {
	mutex_lock(&data->lock);
	delay = 1000 / data->sampling_rate;
	lux = stk_als22x7_get_lux(data);
	stk_als22x7_report(data, lux);
	if (data->enabled == 0)
	    break;
	mutex_unlock(&data->lock);
	msleep(delay);
    };
    mutex_unlock(&data->lock);
    complete(&data->thread_completion);

    return 0;
}

static int32_t stk_als22x7_enable(struct stk_als22x7_data *data) {
    int32_t ret = 0;
    if (data->enabled == 0) {
	ret = stk_als22x7_set_power_state(data, 0);
	if (ret != 0) {
	    ERR("failed to start hardware\n");
	    return ret;
	}
	data->lux_last = 0;
	data->enabled = 1;
	data->thread = kthread_run(stk_als22x7_poll, data, "stk_als22x7_poll");
    } else {
	WARNING("thread is running already\n");
    }
    return ret;
}

static int32_t stk_als22x7_disable(struct stk_als22x7_data *data) {
    int32_t ret = 0;
    if (data->enabled == 1) {
	data->enabled = 0;
	mutex_unlock(&data->lock);
	INFO("wait for thread completion\n");
	wait_for_completion(&data->thread_completion);
	INFO("thread stopped\n");
	mutex_lock(&data->lock);
	data->thread = NULL;
	ret = stk_als22x7_set_power_state(data, 1);
	if (ret != 0) {
	    ERR("failed to shutdown hardware\n");
	    return ret;
	}
    } else {
	WARNING("thread wasn't running\n");
    }
    return ret;
}

static ssize_t stk_als22x7_enable_show(struct device *dev, struct device_attribute *attr, char *buf) {
    int32_t enable;
    struct stk_als22x7_data *data = stk_als22x7_device_data;
    mutex_lock(&data->lock);
    enable = data->enabled;
    mutex_unlock(&data->lock);
    return sprintf(buf, "%d\n", enable);
}

static ssize_t stk_als22x7_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    int ret = 0;
    uint32_t value = simple_strtoul(buf, NULL, 10);
    struct stk_als22x7_data *data = stk_als22x7_device_data;
    INFO("enable = %d\n", value);
    mutex_lock(&data->lock);
    if (value) ret = stk_als22x7_enable(data);
    else ret = stk_als22x7_disable(data);
    mutex_unlock(&data->lock);
    if (ret == 0)
	return count;

    ERR("%s failed\n", __FUNCTION__);
    return 0;
}

static DEVICE_ATTR(enable, 0666, stk_als22x7_enable_show, stk_als22x7_enable_store);

static ssize_t stk_als22x7_sampling_rate_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", stk_als22x7_device_data->sampling_rate);
}

static ssize_t stk_als22x7_sampling_rate_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct stk_als22x7_data *data = stk_als22x7_device_data;
    uint32_t value = simple_strtoul(buf, NULL, 10);
    if (value > MAX_SAMPLING_RATE) value = MAX_SAMPLING_RATE;
    mutex_lock(&data->lock);
    data->sampling_rate = value;
    mutex_unlock(&data->lock);
    return 0;
}

static DEVICE_ATTR(sampling_rate, 0666, stk_als22x7_sampling_rate_show, stk_als22x7_sampling_rate_store);

static ssize_t stk_als22x7_transmittance_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", stk_als22x7_device_data->transmittance);
}

static ssize_t stk_als22x7_transmittance_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct stk_als22x7_data *data = stk_als22x7_device_data;
    uint32_t value = simple_strtoul(buf, NULL, 10);
    mutex_lock(&data->lock);
    data->transmittance = value;
    mutex_unlock(&data->lock);
    return 0;
}

static DEVICE_ATTR(transmittance, 0666, stk_als22x7_transmittance_show, stk_als22x7_transmittance_store);

static ssize_t stk_als22x7_change_threshold_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", stk_als22x7_device_data->change_threshold);
}

static ssize_t stk_als22x7_change_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    struct stk_als22x7_data *data = stk_als22x7_device_data;
    uint32_t value = simple_strtoul(buf, NULL, 10);
    mutex_lock(&data->lock);
    data->change_threshold = value;
    mutex_unlock(&data->lock);
    return 0;
}

static DEVICE_ATTR(change_threshold, 0666, stk_als22x7_change_threshold_show, stk_als22x7_change_threshold_store);

static ssize_t stk_als22x7_lux_show(struct device *dev, struct device_attribute *attr, char *buf) {
    int32_t lux;
    struct stk_als22x7_data *data = stk_als22x7_device_data;
    mutex_lock(&data->lock);
    lux = data->lux_last;
    mutex_unlock(&data->lock);
    return sprintf(buf, "%d lux\n", lux);
}

static DEVICE_ATTR(lux, 0444, stk_als22x7_lux_show, NULL);

static ssize_t stk_als22x7_lux_range_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", MAX_REPORT); // stk_als22x7_code2lux(stk_als22x7_device_data, (1 << 16) - 1));
}

static DEVICE_ATTR(lux_range, 0444, stk_als22x7_lux_range_show, NULL);

static ssize_t stk_als22x7_lux_resolution_show(struct device *dev, struct device_attribute *attr, char *buf) {
    return sprintf(buf, "1\n");
}

static DEVICE_ATTR(lux_resolution, 0444, stk_als22x7_lux_resolution_show, NULL);

static struct device_attribute* stk_als22x7_attrs[] = {
    &dev_attr_enable,
    &dev_attr_sampling_rate,
    &dev_attr_transmittance,
    &dev_attr_change_threshold,
    &dev_attr_lux,
    &dev_attr_lux_range,
    &dev_attr_lux_resolution,
    NULL,
};

static int stk_als22x7_sysfs_create_files(struct device *dev, struct device_attribute **attrs) {
    int err;
    for (; *attrs != NULL; ++attrs) {
        err = device_create_file(dev, *attrs);
        if (err)
            return err;
    }
    return 0;
}

static void stk_als22x7_sysfs_remove_files(struct device *dev, struct device_attribute **attrs) {
    for (; *attrs != NULL; ++attrs)
	device_remove_file(dev, *attrs);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void stk_als22x7_early_suspend(struct early_suspend *h) {
    /*struct stk_als22x7_data *data = container_of(h, struct stk_als22x7_data, early_suspend);
      stk_als22x7_disable(data);*/
    printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
}

static void stk_als22x7_late_resume(struct early_suspend *h) {
    /*struct stk_als22x7_data *data = container_of(h, struct stk_als22x7_data, early_suspend);
      stk_als22x7_enable(data);*/
    printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
}
#else
#ifdef CONFIG_PM
static int stk_als22x7_suspend(struct i2c_client *client){
    struct stk_als22x7_data *data = (struct stk_als22x7_data*)i2c_get_clientdata(client);
    stk_als22x7_disable(data);
    printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
    return 0;
}

static int stk_als22x7_resume(struct i2c_client *client){
    struct stk_als22x7_data *data = (struct stk_als22x7_data*)i2c_get_clientdata(client);
    stk_als22x7_enable(data);
    printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
    return 0;
}
#else
#define stk_als_suspend NULL
#define stk_als_resume NULL
#endif
#endif

static int stk_als22x7_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    int err;
    int32_t n;
    struct i2c_client *client2;
    struct stk_als22x7_data *data = NULL;

    if (stk_als22x7_device_data) {
	ERR("one stk_als22x7 device is already registered. aborting");
	return -EINVAL;
    }

    INFO("probing master address 0x%02x\n", client->addr);
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
	ERR("no support for I2C_FUNC_SMBUS_BYTE_DATA\n");
	return -ENODEV;
    }
    n = i2c_smbus_read_byte(client);
    if (n < 0) {
	ERR("no device found at master address\n");
	return -ENODEV;
    }

    client2 = i2c_new_dummy(client->adapter, client->addr + 1);
    if (client2 == NULL) {
	ERR("can't attach client for slave address\n");
	return -ENOMEM;
    }
    INFO("probing slave address 0x%02x\n", client2->addr);
    if (!i2c_check_functionality(client2->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
	ERR("no support for I2C_FUNC_SMBUS_BYTE_DATA\n");
	i2c_unregister_device(client2);
	return -ENODEV;
    }
    n = i2c_smbus_read_byte(client2);
    if (n < 0) {
	ERR("no device found at slave address, but at master. strange\n");
	i2c_unregister_device(client2);
	return -ENODEV;
    }

    data = kzalloc(sizeof(struct stk_als22x7_data), GFP_KERNEL);
    if (data == NULL) {
	i2c_unregister_device(client2);
	return -ENOMEM;
    }

    /* set up master client */
    data->client1 = client;
    i2c_set_clientdata(client, data);
    mutex_init(&data->lock);
    i2c_smbus_write_byte(client, 0x11);
    data->it = 3;
    data->gain = 2;
    data->reg1 = 75;
    data->reg2 = 0x40;

    /* set up slave client */
    data->client2 = client2;
    i2c_set_clientdata(data->client2, data);
    data->sampling_rate = DEFAULT_SAMPLING_RATE;
    data->transmittance = DEFAULT_TRANSMITTANCE;
    data->change_threshold = DEFAULT_CHANGE_THRESHOLD;
    data->enabled = 0;

    /* finally initialize hardware */
    stk_als22x7_init_hardware(data);	

    /* set up input device */
    data->input_dev = input_allocate_device();
    if (data->input_dev == NULL) {
	ERR("failed to allocate input device\n");
	mutex_destroy(&data->lock);
	kfree(data);
	return -ENOMEM;
    }
    data->input_dev->name = DEVICE_NAME;
    input_set_capability(data->input_dev, EV_ABS, ABS_MISC);
    input_set_abs_params(data->input_dev, ABS_MISC, 0, MAX_REPORT, 0, 0);
//  input_set_abs_params(data->input_dev, ABS_MISC, 0, stk_als22x7_code2lux(data, (1 << 16) - 1), 0, 0);
    err = input_register_device(data->input_dev);
    if (err < 0) {
	ERR("can not register input device\n");
	input_free_device(data->input_dev);
	mutex_destroy(&data->lock);
	kfree(data);
	return err;
    }
    INFO("register input device OK\n");

    /* create sysfs entries only if we really found a device */
    err = stk_als22x7_sysfs_create_files(&data->client1->dev, stk_als22x7_attrs);
    if (err) {
	return -ENOMEM;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
    data->early_suspend.suspend = stk_als22x7_early_suspend;
    data->early_suspend.resume = stk_als22x7_late_resume;
    register_early_suspend(&data->early_suspend);
#endif

    stk_als22x7_device_data = data;

    return 0;
}

static int __devexit stk_als22x7_remove(struct i2c_client *client) {
    struct stk_als22x7_data *data = (struct stk_als22x7_data*)i2c_get_clientdata(client);

    /* stop thread if still running */
    INFO("remove called\n");
    stk_als22x7_disable(data);

    if (data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
        unregister_early_suspend(&data->early_suspend);
#endif
	stk_als22x7_sysfs_remove_files(&data->client1->dev, stk_als22x7_attrs);
        input_unregister_device(data->input_dev);
        input_free_device(data->input_dev);
	if (data->client2)
	    i2c_unregister_device(data->client2);
#if 0
	if (data->client1)
	    i2c_unregister_device(data->client1);
#endif
	mutex_destroy(&data->lock);
	kfree(data);
    }
    stk_als22x7_device_data = NULL;

    return 0;
}

static const struct i2c_device_id stk_als22x7_id[] = {
    { DRIVER_NAME, 0},
    { /* end of list */ }
};
MODULE_DEVICE_TABLE(i2c, stk_als22x7_id);

static struct i2c_driver stk_als22x7_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = DRIVER_NAME,
    },
    .probe    = stk_als22x7_probe,
    .remove   = __devexit_p(stk_als22x7_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend  = stk_als22x7_suspend,
    .resume   = stk_als22x7_resume,
#endif
    .id_table = stk_als22x7_id,
};

static int __init stk_als22x7_init(void)
{
	return i2c_add_driver(&stk_als22x7_driver);
}

static void __exit stk_als22x7_exit(void)
{
	i2c_del_driver(&stk_als22x7_driver);
}

module_init(stk_als22x7_init);
module_exit(stk_als22x7_exit);

MODULE_AUTHOR("Patrick Chang <patrick_chang@sitronix.com>,"
	      "Stephan Kanthak <stylon@gmx.de>");
MODULE_DESCRIPTION("SenseTek / Sitronix ambient light sensor driver");
MODULE_LICENSE("GPL");

