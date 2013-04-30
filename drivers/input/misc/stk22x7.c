/*
 *  stk_i2c_als22x7.c - Linux kernel modules for ambient light sensor
 *
 *  Copyright (C) 2011 Patrick Chang / SenseTek <patrick_chang@sitronix.com.tw>
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
 */


#include <linux/module.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>

#define SENSOR_NAME                     "stk_als"
#define DEVICE_NAME                     "stk-oss"
#define ALS_NAME                        "lightsensor-level"

#define STK_ENABLE_SENSOR_USE_BINARY_SYSFS 0

#define __DEBUG_SYSFS_BIN 1
// #define ENABLE_VERBOSE_PRINTK 1

#define ps_enable_bin_path              "/sys/devices/platform/stk-oss/ps_enable_bin"
#define als_enable_bin_path             "/sys/devices/platform/stk-oss/als_enable_bin"
#define ps_distance_mode_bin_path       "/sys/devices/platform/stk-oss/ps_dist_mode_bin"
#define ps_distance_range_bin_path      "/sys/devices/platform/stk-oss/distance_range_bin"
#define als_lux_range_bin_path          "/sys/devices/platform/stk-oss/lux_range_bin"

#define ps_enable_path                  "/sys/devices/platform/stk-oss/ps_enable"
#define als_enable_path                 "/sys/devices/platform/stk-oss/als_enable"
#define ps_distance_mode_path           "/sys/devices/platform/stk-oss/dist_mode"
#define ps_distance_range_path          "/sys/devices/platform/stk-oss/distance_range"
#define als_lux_range_path              "/sys/devices/platform/stk-oss/lux_range"

#define STK_DRIVER_VER                  "1.5.8"

#define EVENT_TYPE_PROXIMITY            ABS_DISTANCE
#define EVENT_TYPE_LIGHT                ABS_MISC

#define ODR_DELAY                       (1000/CONFIG_STK_ALS_SAMPLING_RATE)

#define stk_writeb i2c_smbus_write_byte
#define stk_readb i2c_smbus_read_byte

#define ABS(x) ((x)>=0? (x):(-x))

#define STK_LOCK0 mutex_unlock(&stkals_io_lock)
#define STK_LOCK1 mutex_lock(&stkals_io_lock)
#define STK_LOCK(x) STK_LOCK##x

#define STK_ALS_I2C_SLAVE_ADDR1 (0x20>>1)
#define STK_ALS_I2C_SLAVE_ADDR2 (0x22>>1)


/* Define CMD */
#define STK_ALS_CMD1_GAIN(x) ((x)<<6)
#define STK_ALS_CMD1_IT(x) ((x)<<2)
#define STK_ALS_CMD1_WM(x) ((x)<<1)
#define STK_ALS_CMD1_SD(x) ((x)<<0)
#define STK_ALS_CMD2_FDIT(x) ((x)<<5)

#define STK_ALS_CMD1_GAIN_MASK 0xC0
#define STK_ALS_CMD1_IT_MASK 0x0C
#define STK_ALS_CMD1_WM_MASK 0x02
#define STK_ALS_CMD1_SD_MASK 0x1
#define STK_ALS_CMD2_FDIT_MASK 0xE


#define ERR(format, args...) \
	printk(KERN_ERR "%s: " format, DEVICE_NAME, ## args)
#define WARNING(format, args...) \
	printk(KERN_WARNING "%s: " format, DEVICE_NAME, ## args)
#ifdef ENABLE_VERBOSE_PRINTK
#define INFO(format, args...) \
	printk(KERN_INFO "%s: " format, DEVICE_NAME, ## args)
#else
#define INFO(format,args...)
#endif

#define __ATTR_BIN(_name,_mode,_read,_write,_size) { \
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.read	= _read,					\
	.write	= _write,					\
	.size = _size,                      \
}
#define __ATTR_BIN_RO(_name,_read,_size) __ATTR_BIN(_name,0444,_read,NULL,_size)
#define __ATTR_BIN_RW(_name,_read,_write,_size) __ATTR_BIN(_name,0666,_read,_write,_size)

#define stk_bin_sysfs_read(name) name##_read(struct file *fptr,struct kobject *kobj, struct bin_attribute *bin_attr,	char * buffer, loff_t offset, size_t count)
#define stk_bin_sysfs_write(name) name##_write(struct file *fptr,struct kobject *kobj, struct bin_attribute *bin_attr,char * buffer, loff_t offset, size_t count)


struct stkals22x7_data {
	struct i2c_client *client1;
	struct i2c_client *client2;
	struct input_dev* input_dev;
	uint32_t gain;
	uint32_t it;
	uint32_t fdit;
	unsigned int power_down_state : 1;
	int32_t als_lux_last;
	uint32_t als_delay;
	uint8_t bThreadRunning;
	uint8_t reg1,reg2;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static struct platform_device *stk_oss_dev = NULL; /* Device structure */

#define ALS_MIN_DELAY 250

#ifdef CONFIG_HAS_EARLYSUSPEND
static void stk_early_suspend(struct early_suspend *h);
static void stk_late_resume(struct early_suspend *h);
#endif

static uint32_t init_all_setting(void);
static int32_t get_lux(void);
static int32_t set_it(uint32_t it);
static int32_t set_gain(uint32_t gain);
static int32_t enable_als(int enable);
// static uint32_t get_enable(void);


static int polling_function(void* arg);
static struct task_struct *polling_tsk;
static struct mutex stkals_io_lock;
static struct completion thread_completion;
static struct stkals22x7_data* pStkAlsData = NULL;


static int32_t als_transmittance = CONFIG_STK_ALS_TRANSMITTANCE;
static uint32_t isStk22x7;

static atomic_t pause_polling = ATOMIC_INIT(0);

static uint32_t init_all_setting(void) {
	int err;
	set_it(2); //x2
	set_gain(1); //x2
	err = stk_writeb(pStkAlsData->client2,64);
	printk("i2c write status when init_all_setting: %d\n", err);
	return true;
}

static int32_t set_it(uint32_t it) {
	pStkAlsData->it = it;
	pStkAlsData->reg1 &= (~STK_ALS_CMD1_IT_MASK);
	pStkAlsData->reg1 |= STK_ALS_CMD1_IT(it);
	return stk_writeb(pStkAlsData->client1,pStkAlsData->reg1);
}

static int32_t set_gain(uint32_t gain) {
	pStkAlsData->gain = gain;
	pStkAlsData->reg1 &= (~STK_ALS_CMD1_GAIN_MASK);
	pStkAlsData->reg1 |= STK_ALS_CMD1_GAIN(gain);
	return stk_writeb(pStkAlsData->client1,pStkAlsData->reg1);
}

static int32_t stkals_set_power_state(unsigned int shutdown) {
	pStkAlsData->reg1 &= (~STK_ALS_CMD1_SD_MASK);
	pStkAlsData->reg1 |= STK_ALS_CMD1_SD(shutdown);
	pStkAlsData->power_down_state = shutdown;
	if (shutdown == 0)
		mdelay (500);
	return stk_writeb(pStkAlsData->client1,pStkAlsData->reg1);
}

inline void report_event(struct input_dev* dev,int32_t report_value) {
	input_report_abs(dev, ABS_MISC, report_value);
	input_sync(dev);
	INFO("STK ALS : als input event %d lux\n",report_value);
}

inline void update_and_check_report_als(int32_t lux) {
	int32_t lux_last;
	lux_last = pStkAlsData->als_lux_last;

	if (unlikely(abs(lux - lux_last)>=CONFIG_STK_ALS_CHANGE_THRESHOLD)) {
		pStkAlsData->als_lux_last = lux;
		report_event(pStkAlsData->input_dev,lux);
	}
}

static int polling_function(void* arg) {
	uint32_t lux;
	uint32_t delay;
	init_completion(&thread_completion);
	while (1) {
		STK_LOCK(1);
		delay = pStkAlsData->als_delay;
		if (!atomic_read(&pause_polling)) {
			lux = get_lux();
			update_and_check_report_als(lux);
		}
		if (pStkAlsData->bThreadRunning == 0)
			break;
		STK_LOCK(0);
		msleep(delay);
	};

	STK_LOCK(0);
	complete(&thread_completion);

	return 0;
}

static int32_t enable_als(int enable) {
	int32_t ret;

	ret = stkals_set_power_state(enable?0:1);
	if (ret != 0) {
		printk(KERN_ERR "enable_als: Failed to set power state to %u\n", enable);
		return ret;
	}
	
	if (enable) {
		if (pStkAlsData->bThreadRunning == 0) {
			pStkAlsData->als_lux_last = 0;
			pStkAlsData->bThreadRunning = 1;
			polling_tsk = kthread_run(polling_function,NULL,"als_polling");
		}
		else {
		    WARNING("STK_ALS : thread has running\n");
		}
	}
	else {
		if (pStkAlsData->bThreadRunning) {
			pStkAlsData->bThreadRunning = 0;
			STK_LOCK(0);
			wait_for_completion(&thread_completion);
			STK_LOCK(1);
			polling_tsk = NULL;
		}
	}

	return ret;
}

#if 0
static uint32_t get_enable(void) {
	return (pStkAlsData->power_down_state)?0:1;
}
#endif

inline int32_t alscode2lux(uint32_t alscode) {
	alscode = (alscode<<9) + (alscode<<6);
	// x 576
	// gain + it --> x 10.64 ==> total ~ x 6000
	// Org : 1 code = 0.6 lux
	// ==> 6000 code = 0.6 lux  (--> Transmittance Factor = 10,000 if directly incident)
	alscode/=als_transmittance;

	return alscode;
}

inline uint32_t lux2alscode(uint32_t lux) {
	lux*=als_transmittance;
	lux/=576;
	if (unlikely(lux>=(1<<16)))
		lux = (1<<16) -1;
	return lux;
}

static int32_t get_lux() {
	int32_t raw;
	stk_writeb(pStkAlsData->client2,(uint8_t)(pStkAlsData->als_lux_last&0x0f)|pStkAlsData->reg2);
	raw = (stk_readb(pStkAlsData->client1) << 8) | stk_readb(pStkAlsData->client2);
	//1 code = 0.6 lux (for 100ms GAIN = 00)
	// x10.64
	return alscode2lux(raw);
}

static ssize_t lux_range_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) {
	return sprintf(buf, "%d\n", alscode2lux((1<<16) -1));
}

static ssize_t lux_res_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) {
	return sprintf(buf, "1\n");
}

#if 0
static ssize_t driver_version_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) {
	return sprintf(buf, "%s\n", STK_DRIVER_VER);
}

static ssize_t sensor_real_name_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) {
	if (isStk22x7)
		return sprintf(buf,"%s\n","STK2207");
	return sprintf(buf,"%s\n","CM3263");
}
#endif


static ssize_t als_enable_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) {
	int32_t enable;
	STK_LOCK(1);
	enable = pStkAlsData->bThreadRunning;
	STK_LOCK(0);
	return sprintf(buf, "%d\n", enable);
}

static ssize_t als_enable_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len) {
	uint32_t value = simple_strtoul(buf, NULL, 10);
	int ret = 0;
	INFO("STK ALS Driver : Enable ALS : %d\n",value);
	STK_LOCK(1);
	ret = enable_als(value);
	STK_LOCK(0);
	if (ret == 0)
		return len;
	else
		return 0;
}

static ssize_t lux_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) {
	int32_t lux;
	STK_LOCK(1);
	lux = pStkAlsData->als_lux_last;
	STK_LOCK(0);
	return sprintf(buf, "%d lux\n", lux);
}

static ssize_t lux_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len) {
	unsigned long value = simple_strtoul(buf, NULL, 10);
	STK_LOCK(1);
	report_event(pStkAlsData->input_dev,value);
	STK_LOCK(0);
	return len;
}

ssize_t stk_bin_sysfs_read(als_lux_range)
{
	uint32_t* pDst = (uint32_t*)buffer;
#if __DEBUG_SYSFS_BIN
	if (count != sizeof(uint32_t)) {
		WARNING("STK ALS Driver : Error --> Read Lux Range(bin) size !=4\n");
		return 0;
	}
#endif
	*pDst = alscode2lux((1<<16)-1);
	return sizeof(uint32_t);
}

ssize_t stk_bin_sysfs_read(als_lux_resolution)
{
	uint32_t* pDst = (uint32_t*)buffer;
#if __DEBUG_SYSFS_BIN
	if (count != sizeof(uint32_t)) {
		WARNING("STK ALS Driver : Error --> Read Distance Range(bin) size !=4\n");
		return 0;
	}
#endif
	// means 1 lux for Android
	*pDst = 1;
	return sizeof(uint32_t);
}

ssize_t stk_bin_sysfs_read(lux_bin) {
	int32_t *pDst = (int32_t*)buffer;
#if __DEBUG_SYSFS_BIN
	if (count != sizeof(uint32_t)) {
		WARNING("STK ALS Driver : Error --> Read Lux(bin) size !=4\n");
		return 0;
	}
#endif
	STK_LOCK(1);
	*pDst = pStkAlsData->als_lux_last;
	STK_LOCK(0);
	return sizeof(uint32_t);
}

ssize_t stk_bin_sysfs_read(als_enable) {
#if __DEBUG_SYSFS_BIN
	if (count != sizeof(uint8_t)) {
		WARNING("STK ALS Driver : Error --> Read als_enable_bin size !=1\n");
		return 0;
	}
#endif
	STK_LOCK(1);
	buffer[0] = pStkAlsData->bThreadRunning?1:0;
	STK_LOCK(0);
	return sizeof(uint8_t);
}

ssize_t  stk_bin_sysfs_write(als_enable) {
	int ret = 0;
#if __DEBUG_SYSFS_BIN
	INFO("STK ALS Driver : Enable ALS : %d\n",(int32_t)(buffer[0]));
#endif
	STK_LOCK(1);
	ret = enable_als(buffer[0]);
	STK_LOCK(0);
	if (!ret)
		printk (KERN_ERR "%s failed\n", __FUNCTION__);
	return count;
}

ssize_t stk_bin_sysfs_read(als_delay) {
#if __DEBUG_SYSFS_BIN
	if (count != sizeof(uint32_t)) {
		WARNING("STK ALS Driver : Error --> Read als_delay size !=4\n");
		return 0;
	}
#endif
	STK_LOCK(1);
	*((uint32_t*)buffer) = pStkAlsData->als_delay;
	STK_LOCK(0);
	return sizeof(uint32_t);
}

ssize_t stk_bin_sysfs_write(als_delay) {
	uint32_t delay;
#if __DEBUG_SYSFS_BIN
	INFO("STK ALS Driver : Set ALS Delay: %d\n",*((int32_t*)buffer));
#endif
	delay = *((uint32_t*)buffer);
	if (delay<ALS_MIN_DELAY)
		delay = ALS_MIN_DELAY;
	STK_LOCK(1);
	pStkAlsData->als_delay = delay;
	STK_LOCK(0);
	return count;
}

ssize_t stk_bin_sysfs_read(als_min_delay) {
#if __DEBUG_SYSFS_BIN
	if (count != sizeof(uint32_t)) {
		WARNING("STK ALS Driver : Error --> Read als_min_delay size !=4\n");
		return 0;
	}
#endif
	*((uint32_t*)buffer) = ALS_MIN_DELAY;
	return sizeof(uint32_t);
}

static struct kobj_attribute lux_range_attribute = (struct kobj_attribute)__ATTR_RO(lux_range);
static struct kobj_attribute lux_attribute = (struct kobj_attribute)__ATTR(lux,0666,lux_show,lux_store);
static struct kobj_attribute als_lux_res_attribute = (struct kobj_attribute)__ATTR_RO(lux_res);
static struct kobj_attribute als_enable_attribute = (struct kobj_attribute)__ATTR(als_enable,0666,als_enable_show,als_enable_store);

static struct bin_attribute als_lux_range_bin_attribute = __ATTR_BIN_RO(lux_range_bin,als_lux_range_read,sizeof(uint32_t));
static struct bin_attribute als_lux_bin_attribute = __ATTR_BIN_RO(lux_bin,lux_bin_read,sizeof(uint32_t));
static struct bin_attribute als_lux_res_bin_attribute = __ATTR_BIN_RO(lux_resolution_bin,als_lux_resolution_read,sizeof(uint32_t));
static struct bin_attribute als_enable_bin_attribute = __ATTR_BIN_RW(als_enable_bin,als_enable_read,als_enable_write,sizeof(uint8_t));

static struct attribute* sensetek_optical_sensors_attrs [] = {
	&lux_range_attribute.attr,
	&lux_attribute.attr,
	&als_enable_attribute.attr,
	&als_lux_res_attribute.attr,
	NULL,
};

static struct bin_attribute* sensetek_optical_sensors_bin_attrs[] = {
	&als_lux_range_bin_attribute,
	&als_lux_bin_attribute,
	&als_lux_res_bin_attribute,
	&als_enable_bin_attribute,
//	&als_delay_bin_attribute,
//	&als_min_delay_bin_attribute,
	NULL,
};

static int stk_sysfs_create_files(struct kobject *kobj,struct attribute** attrs) {
	int err;
	while(*attrs!=NULL) {
		err = sysfs_create_file(kobj,*attrs);
		if (err)
			return err;
		attrs++;
	}
	return 0;
}

static int stk_sysfs_create_bin_files(struct kobject *kobj,struct bin_attribute** bin_attrs) {
	int err;
	while(*bin_attrs!=NULL) {
		err = sysfs_create_bin_file(kobj,*bin_attrs);
		if (err)
			return err;
		bin_attrs++;
	}
	return 0;
}

struct i2c_client *g_client;
static struct i2c_driver stk_als_driver;
static char bLight_Sensor_Exist = 0;

/*
 * Called when a stk als device is matched with this driver
 */
int Light_Sensor_Exist;
EXPORT_SYMBOL(Light_Sensor_Exist);

static int stk_als_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	int err;
	int32_t nRead;
	struct stkals22x7_data*  als_data;

	g_client = client;
	Light_Sensor_Exist = 1;

	INFO("STKALS -- %s: I2C is probing (%s)%d\n", __func__,id->name,(int)id->driver_data);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		ERR("STKALS -- No Support for I2C_FUNC_SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}
	nRead = stk_readb(client);

	if (nRead < 0) {
		Light_Sensor_Exist=0;
		if (bLight_Sensor_Exist)
			init_all_setting();	
		printk("?????????????STKALS : no device found??????????????\n");
		return -ENODEV;
	}


	if (id->driver_data == 1) {
		als_data = kzalloc(sizeof(struct stkals22x7_data),GFP_KERNEL);
		als_data->client1 = client;
		i2c_set_clientdata(client,als_data);
		mutex_init(&stkals_io_lock);
		stk_writeb(client,0x11);
		pStkAlsData = als_data;
		als_data->it = 3;
		als_data->gain = 2;
		als_data->reg1 = 75;
		als_data->reg2 = 0x40;
		bLight_Sensor_Exist = 1;
		return 0;
	}

	if (id->driver_data == 3) {
		nRead = stk_readb(client);
		init_all_setting();
		pr_info("\n\n turn off engineer  driver_data ==  %s  \n\n\n",__func__);
		isStk22x7 = 1;
		als_transmittance = CONFIG_STK_ALS_TRANSMITTANCE;
		return 0;
	}

	if (pStkAlsData) {
		pStkAlsData->client2 = client;
		als_data = pStkAlsData;
		i2c_set_clientdata(client,pStkAlsData);
		pStkAlsData->als_delay = ODR_DELAY;

		pStkAlsData->input_dev = input_allocate_device();
		if (pStkAlsData->input_dev==NULL) {
			mutex_destroy(&stkals_io_lock);
			kfree(pStkAlsData);
			return -ENOMEM;
		}
		isStk22x7 = 0;
		als_transmittance = CONFIG_CM_ALS_TRANSMITTANCE;
		pStkAlsData->input_dev->name = ALS_NAME;

		set_bit(EV_ABS, pStkAlsData->input_dev->evbit);
		input_set_abs_params(pStkAlsData->input_dev, ABS_MISC, 0, alscode2lux((1<<16)-1), 0, 0);
		err = input_register_device(pStkAlsData->input_dev);
		if (err<0) {
			ERR("STK ALS : can not register als input device\n");
			mutex_destroy(&stkals_io_lock);
			input_free_device(pStkAlsData->input_dev);
			kfree(pStkAlsData);
			return err;
		}
		INFO("STK ALS : register als input device OK\n");
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	als_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	als_data->early_suspend.suspend = stk_early_suspend;
	als_data->early_suspend.resume = stk_late_resume;
	register_early_suspend(&als_data->early_suspend);
#endif
	return 0;
}

static int stk_als_remove(struct i2c_client *client)
{
	mutex_destroy(&stkals_io_lock);
	if (pStkAlsData) {
		input_unregister_device(pStkAlsData->input_dev);
		input_free_device(pStkAlsData->input_dev);
		kfree(pStkAlsData);
		pStkAlsData = 0;
	}
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void stk_early_suspend(struct early_suspend *h)
{
	//int err = 0;
	struct stkals22x7_data*  data;
	printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
	data = container_of(h, struct stkals22x7_data, early_suspend);	
	atomic_set(&pause_polling, 1);
}

static void stk_late_resume(struct early_suspend *h)
{
	//int err = 0;
	struct stkals22x7_data*  data;
	printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
	data = container_of(h, struct stkals22x7_data, early_suspend);
	atomic_set(&pause_polling, 0);
	init_all_setting();
}
#endif

static const struct i2c_device_id stk_als_id[] = {
	{ "stk_als22x7_addr1", 1},
	{ "stk_als22x7_addr2", 2},
	{ "stk_als22x7_addr3", 3},
	{}
};
MODULE_DEVICE_TABLE(i2c, stk_als_id);

#ifdef CONFIG_PM
static int stk_als_suspend(struct i2c_client*client, pm_message_t mesg) {
	printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
	return 0;
}

static int stk_als_resume(struct i2c_client *client) {
	printk("!!!!!!!%s!!!!!!!!!!\n",__func__);
	return 0;
}
#endif

static void stk_als_shutdown(struct i2c_client *client) {
	printk(KERN_DEBUG "%s\n", __func__);
        atomic_set(&pause_polling, 1);
}

static struct i2c_driver stk_als_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = SENSOR_NAME,
	},
	.class		= I2C_CLASS_HWMON,
	.id_table = stk_als_id,
	.probe = stk_als_probe,
	.remove = stk_als_remove,
	.shutdown = stk_als_shutdown,
#ifdef CONFIG_PM
	.suspend = stk_als_suspend,
	.resume = stk_als_resume,
#endif
};

static int __init stk_i2c_als22x7_init(void) {
	int ret;

	ret = i2c_add_driver(&stk_als_driver);
	if (ret)
		return ret;
	if (pStkAlsData == NULL)
		return -EINVAL;

	stk_oss_dev = platform_device_alloc(DEVICE_NAME,-1);
	if (!stk_oss_dev)
		goto err;

	if (platform_device_add(stk_oss_dev))
		goto err;

	ret = stk_sysfs_create_bin_files(&(stk_oss_dev->dev.kobj),sensetek_optical_sensors_bin_attrs);
	if (ret)
		goto err;

	ret = stk_sysfs_create_files(&(stk_oss_dev->dev.kobj),sensetek_optical_sensors_attrs);
	if (ret)
		goto err;

	INFO("STK ALS Module initialized.\n");
	return 0;

err:
	i2c_del_driver(&stk_als_driver);
	return -ENOMEM;
}

static void __exit stk_i2c_als22x7_exit(void) {
	i2c_del_driver(&stk_als_driver);
	pStkAlsData = NULL;
}

module_init(stk_i2c_als22x7_init);
module_exit(stk_i2c_als22x7_exit);

MODULE_AUTHOR("Patrick Chang <patrick_chang@sitronix.com>");
MODULE_DESCRIPTION("SenseTek Ambient Light Sensor driver");
MODULE_LICENSE("GPL");
