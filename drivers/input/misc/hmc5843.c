/*
 * Driver for the Honeywell HMC5843 3-Axis Magnetometer.
 *
 * Author: Steven King <sfking@fdwdc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define HMC5843_USE_WORK_QUEUES	1

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/slab.h>

#define	HMC5843_CFG_A_REG			0
#define		HMC5843_CFG_A_BIAS_MASK		0x03
#define		HMC5843_CFG_A_RATE_MASK		0x1c
#define		HMC5843_CFG_A_RATE_SHIFT	2
#define	HMC5843_CFG_B_REG			1
#define		MMC5843_CFG_B_GAIN_MASK		0xe0
#define		MMC5843_CFG_B_GAIN_SHIFT	5
#define	HMC5843_MODE_REG			2
#define		HMC5843_MODE_REPEAT		0
#define		HMC5843_MODE_ONCE		1
#define		HMC5843_MODE_IDLE		2
#define		HMC5843_MODE_SLEEP		3
#define	HMC5843_X_DATA_REG			3
#define	HMC5843_Y_DATA_REG			5
#define	HMC5843_Z_DATA_REG			7
#define	HMC5843_STATUS_REG			9
#define		HMC5843_STATUS_REN		0x04
#define		HMC5843_STATUS_LOCK		0x02
#define		HMC5843_STATUS_RDY		0x01
#define	HMC5843_ID_REG_A			10
#define	HMC5843_ID_REG_B			11
#define	HMC5843_ID_REG_C			12
#define	HMC5843_LAST_REG			12
#define	HMC5843_NUM_REG				13

struct hmc5843 {
	struct i2c_client	*client;
	struct mutex		lock;
	struct input_dev *input_dev;
	struct delayed_work input_work;
	struct input_polled_dev	*ipdev;
	int			bias;
	int			gain;
	int			rate;
	int			mode;
	int			index;

	int 			req_poll_rate;
	int			enable;
};

/* interval between samples for the different rates, in msecs */
static const unsigned int hmc5843_sample_interval[] = {
	1000 * 2,	1000,		1000 / 2,	1000 / 5,
	1000 / 10,	1000 / 20,	1000 / 50,
};

static uint32_t mag_debug;
module_param_named(hmc5843_debug, mag_debug, uint, 0664);

/*
 * From the datasheet:
 *
 * The devices uses an address pointer to indicate which register location is
 * to be read from or written to. These pointer locations are sent from the
 * master to this slave device and succeed the 7-bit address plus 1 bit
 * read/write identifier.
 *
 * To minimize the communication between the master and this device, the
 * address pointer updated automatically without master intervention. This
 * automatic address pointer update has two additional features. First when
 * address 12 or higher is accessed the pointer updates to address 00 and
 * secondly when address 09 is reached, the pointer rolls back to address 03.
 * Logically, the address pointer operation functions as shown below.
 *
 * If (address pointer = 09) then address pointer = 03
 * Else if (address pointer >= 12) then address pointer = 0
 * Else (address pointer) = (address pointer) + 1
 *
 * Since the set of operations performed by this driver is pretty simple,
 * we keep track of the register being written to when updating the
 * configuration and when reading data only update the address ptr when its not
 * pointing to the first data register.
*/

static int hmc5843_write_register(struct hmc5843 *hmc5843, int index)
{
	u8 buf[2];
	int result;

	buf[0] = index;
	if (index == HMC5843_CFG_A_REG)
		buf[1] = hmc5843->bias |
			(hmc5843->rate << HMC5843_CFG_A_RATE_SHIFT);
	else if (index == HMC5843_CFG_B_REG)
		buf[1] = hmc5843->gain << MMC5843_CFG_B_GAIN_SHIFT;
	else if (index == HMC5843_MODE_REG)
		buf[1] = hmc5843->mode;
	else
		return -EINVAL;
	result = i2c_master_send(hmc5843->client, buf, sizeof(buf));

	if (result != 2) {
		hmc5843->index = -1;
		return result;
	}
	hmc5843->index = index + 1;
	return 0;
}

static int hmc5843_read_xyz(struct hmc5843 *hmc5843, int *x, int *y, int *z)
{
	struct i2c_msg msgs[2];
	u8 buf[7];
	int n = 0;
	int result;

	if (hmc5843->index != HMC5843_X_DATA_REG) {
		buf[0]		= HMC5843_X_DATA_REG;

		msgs[0].addr	= hmc5843->client->addr;
		msgs[0].flags	= 0;
		msgs[0].buf	= buf;
		msgs[0].len	= 1;

		hmc5843->index	= HMC5843_X_DATA_REG;
		n = 1;
	}
	msgs[n].addr	= hmc5843->client->addr;
	msgs[n].flags	= I2C_M_RD;
	msgs[n].buf	= buf;
	msgs[n].len	= 7;
	++n;

	result = i2c_transfer(hmc5843->client->adapter, msgs, n);
	if (result != n) {
		hmc5843->index = -1;
		return result;
	}

	*x = (((int)((s8)buf[0])) << 8) | buf[1];
	*y = (((int)((s8)buf[2])) << 8) | buf[3];
	*z = (((int)((s8)buf[4])) << 8) | buf[5];
	return 0;
}

/* sysfs stuff */

/* bias */
static ssize_t hmc5843_show_bias(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", hmc5843->bias);
}

static ssize_t hmc5843_store_bias(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);
	unsigned long val;
	int status = count;

	if ((strict_strtol(buf, 10, &val) < 0) || (val > 2))
		return -EINVAL;
	mutex_lock(&hmc5843->lock);
	if (hmc5843->bias != val) {
		hmc5843->bias = val;
		status = hmc5843_write_register(hmc5843, HMC5843_CFG_A_REG);
	}
	mutex_unlock(&hmc5843->lock);
	return status;
}

static DEVICE_ATTR(bias, S_IWUSR | S_IRUGO, hmc5843_show_bias,
		hmc5843_store_bias);

/* rate */
static ssize_t hmc5843_show_rate(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", hmc5843->rate);
}

static ssize_t hmc5843_store_rate(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);
	unsigned long val;
	int status = count;
	int i;

	if (strict_strtol(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&hmc5843->lock);
	if (hmc5843->rate != val) {
		hmc5843->rate = val;
#ifdef HMC5843_USE_WORK_QUEUES
		if (val == 0) {
			/* Set to the fastest speed */
			i = (ARRAY_SIZE(hmc5843_sample_interval) - 1);
		} else {
			for (i = 0; i < ARRAY_SIZE(hmc5843_sample_interval); i++) {
				if (hmc5843_sample_interval[i] == val)
					break;
				/* The array is from slowest to fastest */
				if ((hmc5843_sample_interval[i] < val) &&
				(hmc5843_sample_interval[i + 1] > val))
					break;
			}
		}
		if (mag_debug)
			dev_info(&hmc5843->client->dev, "%s:Rate picked is %d\n",
				__func__, hmc5843_sample_interval[i]);

		hmc5843->req_poll_rate = hmc5843_sample_interval[i];
#else
		if ((val < 0) || (val < 6))
			hmc5843->ipdev->poll_interval = hmc5843_sample_interval[val];
		else
			return -EINVAL;
#endif
		status = hmc5843_write_register(hmc5843, HMC5843_CFG_A_REG);
	}
	mutex_unlock(&hmc5843->lock);
	return status;
}

static DEVICE_ATTR(rate, S_IWUSR | S_IRUGO, hmc5843_show_rate,
		hmc5843_store_rate);

/* gain */
static ssize_t hmc5843_show_gain(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", hmc5843->gain);
}

static ssize_t hmc5843_store_gain(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);
	unsigned long val;
	int status = count;

	if ((strict_strtol(buf, 10, &val) < 0) || (val > 6))
		return -EINVAL;
	mutex_lock(&hmc5843->lock);
	if (hmc5843->gain != val) {
		hmc5843->gain = val;
		status = hmc5843_write_register(hmc5843, HMC5843_CFG_B_REG);
	}
	mutex_unlock(&hmc5843->lock);
	return status;
}

static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, hmc5843_show_gain,
		hmc5843_store_gain);

/* mode */
static ssize_t hmc5843_show_mode(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", hmc5843->mode);
}

static ssize_t hmc5843_store_mode(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);
	unsigned long val;
	int status = count;

	if ((strict_strtol(buf, 10, &val) < 0) || (val > 3))
		return -EINVAL;
	mutex_lock(&hmc5843->lock);
	if (hmc5843->mode != val) {
		hmc5843->mode = val;
		status = hmc5843_write_register(hmc5843, HMC5843_MODE_REG);
	}
	mutex_unlock(&hmc5843->lock);
	return status;
}

static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO, hmc5843_show_mode,
		hmc5843_store_mode);

static ssize_t hmc5843_show_enable(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", hmc5843->enable);
}

static ssize_t hmc5843_store_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);
	unsigned long val;
	int error;

#ifdef HMC5843_USE_WORK_QUEUES
	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	if (val < 0 || val > 1)
		return -EINVAL;

	if (val)
		hmc5843->mode = HMC5843_MODE_REPEAT;
	else
		hmc5843->mode = HMC5843_MODE_SLEEP;

	error = hmc5843_write_register(hmc5843, HMC5843_MODE_REG);

	if (error == 0) {
		if (val == 0)
			cancel_delayed_work_sync(&hmc5843->input_work);
		else
			schedule_delayed_work(&hmc5843->input_work, 0);

		hmc5843->enable = val;
	}
#endif
	return count;
}
static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO, hmc5843_show_enable,
		hmc5843_store_enable);

static struct attribute *hmc5843_attributes[] = {
	&dev_attr_bias.attr,
	&dev_attr_rate.attr,
	&dev_attr_gain.attr,
	&dev_attr_mode.attr,
	&dev_attr_enable.attr,
	NULL,
};

static const struct attribute_group hmc5843_attr_group = {
	.attrs = hmc5843_attributes,
};

#ifdef HMC5843_USE_WORK_QUEUES
static void hmc5843_read_report_data(struct hmc5843 *data)
{
	int x = 0;
	int y = 0;
	int z = 0;

	mutex_lock(&data->lock);
	if (!hmc5843_read_xyz(data, &x, &y, &z)) {
		if (mag_debug)
			dev_info(&data->client->dev,
				"%s:Reporting x= %d, y= %d, z= %d\n",
				__func__, x, y, z);
		input_report_abs(data->input_dev, ABS_HAT0X, x);
		input_report_abs(data->input_dev, ABS_HAT0Y, y);
		input_report_abs(data->input_dev, ABS_BRAKE, z);
		input_sync(data->input_dev);
	}
	mutex_unlock(&data->lock);
}

static void hmc5843_input_work_func(struct work_struct *work)
{
	struct hmc5843 *hmc = container_of((struct delayed_work *)work,
						  struct hmc5843,
						  input_work);

	hmc5843_read_report_data(hmc);
	schedule_delayed_work(&hmc->input_work,
		msecs_to_jiffies(hmc->req_poll_rate));
}

static int __devinit hmc5843_input_init(struct hmc5843 *hmc5843)
{
	int ret;

	hmc5843->input_dev = input_allocate_device();
	if (hmc5843->input_dev == NULL) {
		dev_err(&hmc5843->client->dev,
			"Failed to allocate input device\n");
		return -ENOMEM;
	}

	hmc5843->input_dev->name = "hmc5843";
	hmc5843->input_dev->id.bustype = BUS_I2C;

	 __set_bit(EV_ABS, hmc5843->input_dev->evbit);

	input_set_abs_params(hmc5843->input_dev, ABS_HAT0X, -2047, 2047, 0, 0);
	input_set_abs_params(hmc5843->input_dev, ABS_HAT0Y, -2047, 2047, 0, 0);
	input_set_abs_params(hmc5843->input_dev, ABS_BRAKE, -2047, 2047, 0, 0);

	ret = input_register_device(hmc5843->input_dev);
	if (ret) {
		dev_err(&hmc5843->client->dev,
			"Unable to register input device\n");
		input_free_device(hmc5843->input_dev);
		return ret;
	}
	return 0;
}
static void hmc5843_unregister_input_device(struct hmc5843 *hmc5843)
{
	input_unregister_device(hmc5843->input_dev);
}

#else /* use polled input device */

static void hmc5843_poll(struct input_polled_dev *ipdev)
{
	struct hmc5843 *hmc5843 = ipdev->private;
	int x, y, z;

	mutex_lock(&hmc5843->lock);
	if (!hmc5843_read_xyz(hmc5843, &x, &y, &z)) {
		input_report_abs(ipdev->input, ABS_X, x);
		input_report_abs(ipdev->input, ABS_Y, y);
		input_report_abs(ipdev->input, ABS_Z, z);
		input_sync(ipdev->input);
	}
	mutex_unlock(&hmc5843->lock);
}

static int __devinit hmc5843_input_init(struct hmc5843 *hmc5843)
{
	int status;
	struct input_polled_dev *ipdev;

	ipdev = input_allocate_polled_device();
	if (!ipdev) {
		dev_dbg(&hmc5843->client->dev, "error creating input device\n");
		return -ENOMEM;
	}
	ipdev->poll = hmc5843_poll;
	ipdev->poll_interval = hmc5843_sample_interval[hmc5843->rate];
	ipdev->private = hmc5843;

	ipdev->input->name = "Honeywell HMC5843 3-Axis Magnetometer";
	ipdev->input->phys = "hmc5843/input0";
	ipdev->input->id.bustype = BUS_HOST;

	set_bit(EV_ABS, ipdev->input->evbit);

	input_set_abs_params(ipdev->input, ABS_X, -2047, 2047, 0, 0);
	input_set_abs_params(ipdev->input, ABS_Y, -2047, 2047, 0, 0);
	input_set_abs_params(ipdev->input, ABS_Z, -2047, 2047, 0, 0);

	status = input_register_polled_device(ipdev);
	if (status) {
		dev_dbg(&hmc5843->client->dev,
				"error registering input device\n");
		input_free_polled_device(ipdev);
		goto exit;
	}
	hmc5843->ipdev = ipdev;
exit:
	return status;
}

static void hmc5843_unregister_input_device(struct hmc5843 *hmc5843)
{
	input_unregister_polled_device(hmc5843->ipdev);
}
#endif

static int __devinit hmc5843_device_init(struct hmc5843 *hmc5843)
{
	struct i2c_client *client = hmc5843->client;
	int status;

	struct i2c_msg msgs[2];
	u8 buf[6] = { HMC5843_ID_REG_A }; /* start reading at 1st id reg */

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].buf = buf;
	msgs[0].len = 1;
	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = buf; /* overwrite sent address on read */
	msgs[1].len = 6; /* 3 id regs + cfg_a, cfg_b & mode reg */

	status = i2c_transfer(client->adapter, msgs, 2);
	if (status != 2) {
		dev_dbg(&client->dev, "unable to contact device\n");
		return status;
	}
	if (strncmp(buf, "H43", 3)) {
		dev_dbg(&client->dev, "incorrect device id %-3.3s", buf);
		return -EINVAL;
	}
	/* the read will have wrapped to 0, bytes 3-6 are cfg_a, cfg_b, mode */
	hmc5843->bias = buf[3] & HMC5843_CFG_A_BIAS_MASK;
	hmc5843->rate = buf[3] >> HMC5843_CFG_A_RATE_SHIFT;
	hmc5843->gain = buf[4] >> MMC5843_CFG_B_GAIN_SHIFT;
	hmc5843->mode = buf[5];

	hmc5843->index = 3;
	mutex_init(&hmc5843->lock);
	return 0;
}

static int __devinit hmc5843_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int status;
	struct hmc5843 *hmc5843;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_dbg(&client->dev, "adapter doesn't support I2C\n");
		return -ENODEV;
	}

	hmc5843 = kzalloc(sizeof(*hmc5843), GFP_KERNEL);
	if (!hmc5843) {
		dev_dbg(&client->dev, "unable to allocate memory\n");
		return -ENOMEM;
	}

	hmc5843->client = client;
	i2c_set_clientdata(client, hmc5843);

	hmc5843->req_poll_rate = 200;
	hmc5843->enable = 0;

	status = hmc5843_device_init(hmc5843);
	if (status)
		goto fail0;

	status = hmc5843_input_init(hmc5843);
	if (status)
		goto fail0;

	status = sysfs_create_group(&client->dev.kobj, &hmc5843_attr_group);
	if (status) {
		dev_dbg(&client->dev, "could not create sysfs files\n");
		goto fail1;
	}
	INIT_DELAYED_WORK(&hmc5843->input_work, hmc5843_input_work_func);
	return 0;
fail1:
	hmc5843_unregister_input_device(hmc5843);
fail0:
	kfree(hmc5843);
	return status;
}

static int __devexit hmc5843_i2c_remove(struct i2c_client *client)
{
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &hmc5843_attr_group);
	hmc5843_unregister_input_device(hmc5843);
	i2c_set_clientdata(client, NULL);
	kfree(hmc5843);
	return 0;
}

#ifdef CONFIG_PM
static int hmc5843_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	/* save our current mode for resume and put device to sleep */
	int m = hmc5843->mode;
#ifdef HMC5843_USE_WORK_QUEUES
	cancel_delayed_work_sync(&hmc5843->input_work);
#endif
	hmc5843->mode = HMC5843_MODE_SLEEP;
	hmc5843_write_register(hmc5843, HMC5843_MODE_REG);
	hmc5843->mode = m;
	return 0;
}

static int hmc5843_resume(struct i2c_client *client)
{
	struct hmc5843 *hmc5843 = i2c_get_clientdata(client);

	/* restore whatever mode we were in before suspending */
	if (hmc5843->enable != 0) {
		hmc5843_write_register(hmc5843, HMC5843_MODE_REG);
#ifdef HMC5843_USE_WORK_QUEUES
		schedule_delayed_work(&hmc5843->input_work, 0);
#endif
	}
	return 0;
}

#endif

static const struct i2c_device_id hmc5843_id[] = {
	{ "hmc5843", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, hmc5843_id);

static struct i2c_driver hmc5843_i2c_driver = {
	.driver		= {
		.name	= "hmc5843",
		.owner	= THIS_MODULE,
	},
	.probe		= hmc5843_i2c_probe,
	.remove		= __devexit_p(hmc5843_i2c_remove),
	.id_table	= hmc5843_id,
#ifdef CONFIG_PM
	.suspend = hmc5843_suspend,
	.resume	= hmc5843_resume,
#endif
};

static int __init hmc5843_init(void)
{
	return i2c_add_driver(&hmc5843_i2c_driver);
}
module_init(hmc5843_init);

static void __exit hmc5843_exit(void)
{
	i2c_del_driver(&hmc5843_i2c_driver);
}
module_exit(hmc5843_exit);

MODULE_AUTHOR("Steven King <sfking@fdwdc.com>");
MODULE_DESCRIPTION("Honeywell HMC5843 3-Axis Magnetometer driver");
MODULE_LICENSE("GPL");
