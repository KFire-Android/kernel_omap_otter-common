/**
 * tsl2771.c - ALS and Proximity sensor driver
 *
 * Copyright (C) 2011 Texas Instruments
 * Author: Dan Murphy <DMurphy@ti.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
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
#include <linux/i2c/tsl2771.h>
#include <linux/gpio.h>

#define TSL2771_DEBUG 1

#define TSL2771_ALLOWED_R_BYTES	25
#define TSL2771_ALLOWED_W_BYTES	2
#define TSL2771_MAX_RW_RETRIES	5
#define TSL2771_I2C_RETRY_DELAY 10

#define TSL2771_I2C_WRITE	0x80
#define TSL2771_I2C_READ	0xa0

#define TSL2771_PROX_INT_CLR	0x65
#define TSL2771_ALS_INT_CLR	0x66
#define TSL2771_ALL_INT_CLR	0x67

/* TSL2771 Read only registers */
#define TSL2771_REV	0x11
#define TSL2771_ID	0x12
#define TSL2771_STATUS	0x13
#define TSL2771_CDATAL	0x14
#define TSL2771_CDATAH	0x15
#define TSL2771_IRDATAL	0x16
#define TSL2771_IRDATAH	0x17
#define TSL2771_PDATAL	0x18
#define TSL2771_PDATAH	0x19

/* Enable register mask */
#define TSL2771_PWR_ON		(1 << 0)
#define TSL2771_ADC_EN		(1 << 1)
#define TSL2771_PROX_EN		(1 << 2)
#define TSL2771_WAIT_EN		(1 << 3)
#define TSL2771_ALS_INT_EN	(1 << 4)
#define TSL2771_PROX_INT_EN	(1 << 5)

#define TSL2771_ALS_INT		(1 << 4)
#define TSL2771_PROX_INT	(1 << 5)

#define TSL2771_ALS_EN_FLAG	0x01
#define TSL2771_PROX_EN_FLAG	0x02

struct tsl2771_data {
	struct tsl2771_platform_data *pdata;
	struct i2c_client *client;
	struct input_dev *prox_input_dev;
	struct input_dev *als_input_dev;
	struct mutex enable_mutex;

	int lux;
	int prox_distance;
	int power_state;
	int power_context;	/* Saves the power state for suspend/resume */
	int als_gain;
};

static int als_gain_table[4] = {
	1, 8, 16, 120
};

static uint32_t als_prox_debug;
module_param_named(tsl2771_debug, als_prox_debug, uint, 0664);

#ifdef TSL2771_DEBUG
struct tsl2771_reg {
	const char *name;
	uint8_t reg;
	int writeable;
} tsl2771_regs[] = {
	{ "REV",		TSL2771_REV, 0 },
	{ "CHIP_ID",		TSL2771_ID, 0 },
	{ "STATUS",		TSL2771_STATUS, 0 },
	{ "ADC_LOW",		TSL2771_CDATAL, 0 },
	{ "ADC_HI",		TSL2771_CDATAH, 0 },
	{ "IR_LOW_DATA",	TSL2771_IRDATAL, 0 },
	{ "IR_HI_DATA",		TSL2771_IRDATAH, 0 },
	{ "P_LOW_DATA",		TSL2771_PDATAL, 0 },
	{ "P_HI_DATA",		TSL2771_PDATAH, 0 },
	{ "ENABLE",		TSL2771_ENABLE, 1 },
	{ "A_ADC_TIME",		TSL2771_ATIME, 1 },
	{ "P_ADC_TIME",		TSL2771_PTIME, 1 },
	{ "WAIT_TIME",		TSL2771_WTIME, 1 },
	{ "A_LOW_TH_LOW",	TSL2771_AILTL, 1 },
	{ "A_LOW_TH_HI",	TSL2771_AILTH, 1 },
	{ "A_HI_TH_LOW",	TSL2771_AIHTL, 1 },
	{ "A_HI_TH_HI",		TSL2771_AIHTH, 1 },
	{ "P_LOW_TH_LOW",	TSL2771_PILTL, 1 },
	{ "P_LOW_TH_HI",	TSL2771_PILTH, 1 },
	{ "P_HI_TH_LOW",	TSL2771_PIHTL, 1 },
	{ "P_HI_TH_HI",	TSL2771_PIHTH, 1 },
	{ "INT_PERSIT",		TSL2771_PERS, 1 },
	{ "PROX_PULSE_CNT",	TSL2771_PPCOUNT, 1 },
	{ "CONTROL",		TSL2771_CONTROL, 1 },
};
#endif

static int tsl2771_write_reg(struct tsl2771_data *data, u8 reg,
					u8 val, int len)
{
	int err;
	int tries = 0;
	u8 buf[TSL2771_ALLOWED_W_BYTES];

	struct i2c_msg msgs[] = {
		{
		 .addr = data->client->addr,
		 .flags = data->client->flags,
		 .len = len + 1,
		 },
	};

	buf[0] = (TSL2771_I2C_WRITE | reg);
	/* TO DO: Need to find out if we can write multiple bytes at once over
	* I2C like we can with the read */
	buf[1] = val;

	msgs->buf = buf;

	do {
		err = i2c_transfer(data->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(TSL2771_I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < TSL2771_MAX_RW_RETRIES));

	if (err != 1) {
		dev_err(&data->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tsl2771_read_reg(struct tsl2771_data *data, u8 reg, u8 *buf, int len)
{
	int err;
	int tries = 0;
	u8 reg_buf[TSL2771_ALLOWED_R_BYTES];

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
	reg_buf[0] = (TSL2771_I2C_READ | reg);
	msgs->buf = reg_buf;

	do {
		err = i2c_transfer(data->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(TSL2771_I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < TSL2771_MAX_RW_RETRIES));

	if (err != 2) {
		dev_err(&data->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tsl2771_init_device(struct tsl2771_data *data)
{
	int error = 0;

	error = tsl2771_write_reg(data, TSL2771_CONFIG, data->pdata->config, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_ENABLE,
			data->pdata->def_enable, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_ATIME,
			data->pdata->als_adc_time, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_PTIME,
			data->pdata->prox_adc_time, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_WTIME,
			data->pdata->wait_time, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_AILTL,
			data->pdata->als_low_thresh_low_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_AILTH,
			data->pdata->als_low_thresh_high_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_AIHTL,
			data->pdata->als_high_thresh_low_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_AIHTH,
			data->pdata->als_high_thresh_high_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_PILTL,
			data->pdata->prox_low_thresh_low_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_PILTH,
			data->pdata->prox_low_thresh_high_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_PIHTL,
			data->pdata->prox_high_thresh_low_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_PIHTH,
			data->pdata->prox_high_thresh_high_byte, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_PERS,
			data->pdata->interrupt_persistence, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_PPCOUNT,
			data->pdata->prox_pulse_count, 1);
	if (error)
		goto init_error;

	error = tsl2771_write_reg(data, TSL2771_CONTROL,
			data->pdata->gain_control, 1);
	if (error)
		goto init_error;

	return 0;

init_error:
	pr_err("%s:Failed initializing the device\n", __func__);
	return -1;

}

static int tsl2771_read_prox(struct tsl2771_data *data)
{
	u8 data_buffer[4];
	int prox_data = 0;
	tsl2771_read_reg(data, TSL2771_PDATAL, data_buffer, 2);

	prox_data = (data_buffer[1] << 8);
	prox_data |= data_buffer[0];

	if (als_prox_debug & 0x2)
		pr_info("%s:Prox Data 0x%X\n", __func__, prox_data);

	data->prox_distance = prox_data;

	return prox_data;
}

static int tsl2771_read_als(struct tsl2771_data *data)
{
	int cdata_data = 0;
	int irdata_data = 0;
	int ratio = 0;
	int iac = 0;
	int cpl = 0;
	int integration_time = 0;
	u8 data_buffer[4];

	tsl2771_read_reg(data, TSL2771_CDATAL, data_buffer, 4);

	cdata_data = (data_buffer[1] << 8);
	cdata_data |= data_buffer[0];
	irdata_data = (data_buffer[3] << 8);
	irdata_data |= data_buffer[2];
	if (als_prox_debug & 0x1)
		pr_info("%s: IR Data 0x%X CData 0x%X\n", __func__,
				irdata_data, cdata_data);
	if (!cdata_data) {
		pr_err("%s:cdata is NULL\n", __func__);
		data->lux = 0;
		goto out;
	}

	ratio = (irdata_data * 100) / cdata_data;
	if (als_prox_debug & 0x1)
		pr_info("%s: Ratio is %i\n", __func__, ratio);

	if ((ratio >= 0) && (ratio <= 30))
		iac = ((1000 * cdata_data) - (1846 * irdata_data));
	else if ((ratio >= 30) && (ratio <= 38))
		iac = ((1268 * cdata_data) - (2740 * irdata_data));
	else if ((ratio >= 38) && (ratio <= 45))
		iac = ((749 * cdata_data) - (1374 * irdata_data));
	else if ((ratio >= 45) && (ratio <= 54))
		iac = ((477 * cdata_data) - (769 * irdata_data));

	if (als_prox_debug & 0x1)
		pr_info("%s: IAC %i\n", __func__, iac);

	integration_time = (272 * (256 - data->pdata->als_adc_time));
	data->als_gain = als_gain_table[data->pdata->gain_control & 0x3];
	if (data->pdata->glass_attn && data->pdata->device_factor)
		cpl = ((integration_time * data->als_gain) /
			(data->pdata->glass_attn * data->pdata->device_factor));
	else
		pr_err("%s: Device factor or glass attenuation is NULL\n",
			__func__);

	if (als_prox_debug & 0x1)
		pr_info("%s: CPL %i\n", __func__, cpl);

	if (cpl)
		data->lux = iac / cpl;
	else
		pr_err("%s: Count per lux is zero\n", __func__);

	if (als_prox_debug & 0x1)
		pr_info("%s:Current lux is %i\n", __func__, data->lux);

out:
	return data->lux;
}
static int tsl2771_als_enable(struct tsl2771_data *data, int val)
{
	u8 enable_buf[2];
	u8 write_buf;

	tsl2771_read_reg(data, TSL2771_ENABLE, enable_buf, 1);
	if (val) {
		write_buf = (TSL2771_ALS_INT_EN | TSL2771_ADC_EN |
				TSL2771_PWR_ON | enable_buf[0]);
		data->power_state |= TSL2771_ALS_EN_FLAG;
	} else {
		write_buf = (~TSL2771_ALS_INT_EN & ~TSL2771_ADC_EN &
				enable_buf[0]);

		if (!(data->power_state & ~TSL2771_PROX_EN_FLAG))
			write_buf &= ~TSL2771_PWR_ON;

		data->power_state &= ~TSL2771_ALS_EN_FLAG;
	}

	return tsl2771_write_reg(data, TSL2771_ENABLE, write_buf, 1);

}

static int tsl2771_prox_enable(struct tsl2771_data *data, int val)
{
	u8 enable_buf[2];
	u8 write_buf;

	tsl2771_read_reg(data, TSL2771_ENABLE, enable_buf, 1);
	if (val) {
		write_buf = (TSL2771_PROX_INT_EN | TSL2771_PROX_EN |
				TSL2771_PWR_ON | enable_buf[0]);
		data->power_state |= TSL2771_PROX_EN_FLAG;
	} else {
		write_buf = (~TSL2771_PROX_INT_EN & ~TSL2771_PROX_EN &
				enable_buf[0]);

		if (!(data->power_state & ~TSL2771_ALS_EN_FLAG))
			write_buf &= ~TSL2771_PWR_ON;

		data->power_state &= ~TSL2771_PROX_EN_FLAG;
	}
	return tsl2771_write_reg(data, TSL2771_ENABLE, write_buf, 1);

}

static void tsl2771_report_prox_input(struct tsl2771_data *data)
{
	input_report_abs(data->prox_input_dev, ABS_DISTANCE,
				data->prox_distance);
	input_sync(data->prox_input_dev);
}

static void tsl2771_report_als_input(struct tsl2771_data *data)
{
	input_event(data->als_input_dev, EV_LED, LED_MISC, data->lux);
	input_sync(data->als_input_dev);
}

static irqreturn_t tsl2771_work_queue(int irq, void *dev_id)
{
	struct tsl2771_data *data = dev_id;
	int err = 0;
	u8 enable_buf[2];

	mutex_lock(&data->enable_mutex);
	tsl2771_read_reg(data, TSL2771_STATUS, enable_buf, 1);
	if (enable_buf[0] & TSL2771_ALS_INT) {
		err = tsl2771_read_als(data);
		if (err < 0) {
			pr_err("%s: Not going to report ALS\n", __func__);
			goto prox_check;
		}
		tsl2771_report_als_input(data);
	}

prox_check:
	if (enable_buf[0] & TSL2771_PROX_INT) {
		err = tsl2771_read_prox(data);
		if (err < 0) {
			pr_err("%s: Not going to report prox\n", __func__);
			goto done;
		}
		tsl2771_report_prox_input(data);
	}

done:
	tsl2771_write_reg(data, TSL2771_ALL_INT_CLR, 0, 0);
	mutex_unlock(&data->enable_mutex);
	return IRQ_HANDLED;
}

static ssize_t tsl2771_show_attr_enable(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tsl2771_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", (data->power_state & 0x3));
}

static ssize_t tsl2771_store_attr_prox_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tsl2771_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error = 0;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return error;

	if (!(data->pdata->flags & TSL2771_USE_PROX)) {
		pr_err("%s: PROX is not supported by kernel\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&data->enable_mutex);
	if (!(data->power_state & 0x3)) {
		if (data->pdata->tsl2771_pwr_control) {
			data->pdata->tsl2771_pwr_control(val);
			tsl2771_init_device(data);
			data->power_state |= TSL2771_PROX_EN_FLAG;
		}
	}

	error = tsl2771_prox_enable(data, val);
	if (error) {
		pr_err("%s:Failed to turn prox %s\n",
			__func__, (val ? "on" : "off"));
		goto error;
	}

	error = tsl2771_read_prox(data);
	tsl2771_report_prox_input(data);
error:
	mutex_unlock(&data->enable_mutex);
	return count;
}

static ssize_t tsl2771_store_attr_als_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tsl2771_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error = 0;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return error;

	if (!(data->pdata->flags & TSL2771_USE_ALS)) {
		pr_err("%s: ALS is not supported by kernel\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&data->enable_mutex);
	if (!(data->power_state & 0x3)) {
		if (data->pdata->tsl2771_pwr_control) {
			data->pdata->tsl2771_pwr_control(val);
			tsl2771_init_device(data);
			data->power_state |= TSL2771_ALS_EN_FLAG;
		}
	}


	error = tsl2771_als_enable(data, val);
	if (error) {
		pr_err("%s:Failed to turn prox %s\n",
			__func__, (val ? "on" : "off"));
		goto error;
	}

	error = tsl2771_read_als(data);
	tsl2771_report_als_input(data);
error:
	mutex_unlock(&data->enable_mutex);
	return count;
}

static ssize_t tsl2771_show_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return sprintf(buf, "%d\n", 1);
}

static ssize_t tsl2771_store_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned long interval;
	int error = 0;

	error = kstrtoul(buf, 0, &interval);
	if (error)
		return error;

	return count;
}

#ifdef TSL2771_DEBUG
static ssize_t tsl2771_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tsl2771_data *data = platform_get_drvdata(pdev);
	unsigned i, n, reg_count;
	u8 read_buf[2];

	reg_count = sizeof(tsl2771_regs) / sizeof(tsl2771_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		tsl2771_read_reg(data, tsl2771_regs[i].reg, read_buf, 1);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       tsl2771_regs[i].name,
			       read_buf[0]);
	}

	return n;
}

static ssize_t tsl2771_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tsl2771_data *data = platform_get_drvdata(pdev);
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

	reg_count = sizeof(tsl2771_regs) / sizeof(tsl2771_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, tsl2771_regs[i].name)) {
			if (tsl2771_regs[i].writeable) {
				error = tsl2771_write_reg(data,
						tsl2771_regs[i].reg, value, 1);
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
static ssize_t tsl2771_lux_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tsl2771_data *data = platform_get_drvdata(pdev);

	tsl2771_read_als(data);
	return sprintf(buf, "%d\n", data->lux);
}
static DEVICE_ATTR(registers, S_IWUSR | S_IRUGO,
		tsl2771_registers_show, tsl2771_registers_store);

static DEVICE_ATTR(lux, S_IWUSR | S_IRUGO,
		tsl2771_lux_show, NULL);
#endif
static DEVICE_ATTR(als_enable, S_IWUSR | S_IRUGO,
		tsl2771_show_attr_enable, tsl2771_store_attr_als_enable);

static DEVICE_ATTR(prox_enable, S_IWUSR | S_IRUGO,
		tsl2771_show_attr_enable, tsl2771_store_attr_prox_enable);

static DEVICE_ATTR(delay, S_IWUSR | S_IRUGO,
		tsl2771_show_attr_delay, tsl2771_store_attr_delay);

static struct attribute *tsl2771_attrs[] = {
	&dev_attr_als_enable.attr,
	&dev_attr_prox_enable.attr,
	&dev_attr_delay.attr,
#ifdef TSL2771_DEBUG
	&dev_attr_registers.attr,
	&dev_attr_lux.attr,
#endif
	NULL
};

static const struct attribute_group tsl2771_attr_group = {
	.attrs = tsl2771_attrs,
};

static int __devinit tsl2771_driver_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct tsl2771_platform_data *pdata = client->dev.platform_data;
	struct tsl2771_data *data;
	int ret = 0;
	u8 read_buf[2];

	pr_info("%s: Enter\n", __func__);

	if (pdata == NULL) {
		pr_err("%s: Platform data not found\n", __func__);
		return -ENODEV;
	}

	if (!pdata->flags) {
		pr_err("%s: No function defined in the board file\n",
				__func__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: need I2C_FUNC_I2C\n", __func__);
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct tsl2771_data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto error;
	}
	data->pdata = pdata;
	data->client = client;
	i2c_set_clientdata(client, data);
	mutex_init(&data->enable_mutex);

	if (data->pdata->flags & TSL2771_USE_PROX) {
		data->prox_input_dev = input_allocate_device();
		if (data->prox_input_dev == NULL) {
			ret = -ENOMEM;
			pr_err("%s:Failed to allocate proximity input device\n",
					__func__);
			goto prox_input_error;
		}

		data->prox_input_dev->name = "tsl2771_prox";
		data->prox_input_dev->id.bustype = BUS_I2C;
		data->prox_input_dev->dev.parent = &data->client->dev;
		input_set_capability(data->prox_input_dev,
					EV_ABS, ABS_DISTANCE);
		input_set_drvdata(data->prox_input_dev, data);

		__set_bit(EV_ABS, data->prox_input_dev->evbit);
		input_set_abs_params(data->prox_input_dev,
					ABS_DISTANCE, 0, 1, 0, 0);

		ret = input_register_device(data->prox_input_dev);
		if (ret) {
			pr_err("%s:Unable to register prox device\n", __func__);
			goto prox_register_fail;
		}
	}

	if (data->pdata->flags & TSL2771_USE_ALS) {
		data->als_input_dev = input_allocate_device();
		if (data->als_input_dev == NULL) {
			ret = -ENOMEM;
			pr_err("%s:Failed to allocate als input device\n",
					__func__);
			goto als_input_error;
		}
		data->als_input_dev->name = "tsl2771_als";
		data->als_input_dev->id.bustype = BUS_I2C;
		data->als_input_dev->dev.parent = &data->client->dev;
		input_set_capability(data->als_input_dev, EV_MSC, MSC_RAW);
		input_set_capability(data->als_input_dev, EV_LED, LED_MISC);
		input_set_drvdata(data->als_input_dev, data);
		ret = input_register_device(data->als_input_dev);
		if (ret) {
			pr_err("%s:Unable to register als device\n", __func__);
			goto als_register_fail;
		}
	}

	ret = gpio_request_one(data->client->irq, GPIOF_IN, "sensor");
	if (ret) {
		dev_err(&data->client->dev, "sensor: gpio request failure\n");
		return ret;
	}

	if (data->client->irq) {
		ret = request_threaded_irq(gpio_to_irq(data->client->irq), NULL,
					tsl2771_work_queue,
					data->pdata->irq_flags,
					data->client->name, data);
		if (ret < 0) {
			dev_err(&data->client->dev,
				"request_threaded_irq failed\n");
			goto irq_request_fail;
		}
	} else {
		pr_err("%s: No IRQ defined therefore failing\n", __func__);
		goto irq_request_fail;
	}

	if (data->pdata->tsl2771_pwr_control) {
		data->pdata->tsl2771_pwr_control(1);
		tsl2771_read_reg(data, TSL2771_REV, read_buf, 1);
		pr_info("%s: tsl2771 rev is 0x%X\n", __func__, read_buf[0]);
	} else {
		pr_err("%s: tsl2771 pwr function not defined\n", __func__);
	}

	ret = tsl2771_init_device(data);
	if (ret) {
		pr_err("%s:TSL2771 device init failed\n", __func__);
		goto device_init_fail;
	}

	data->power_state = 0;

	ret = sysfs_create_group(&client->dev.kobj, &tsl2771_attr_group);
	if (ret) {
		pr_err("%s:Cannot create sysfs group\n", __func__);
		goto sysfs_create_fail;
	}

	return 0;

sysfs_create_fail:
	if (data->pdata->tsl2771_pwr_control)
		data->pdata->tsl2771_pwr_control(0);
device_init_fail:
	if (data->client->irq)
		free_irq(data->client->irq, data);
irq_request_fail:
als_register_fail:
	if (data->pdata->flags & TSL2771_USE_ALS)
		input_free_device(data->als_input_dev);
als_input_error:
prox_register_fail:
	if (data->pdata->flags & TSL2771_USE_PROX)
		input_free_device(data->prox_input_dev);
prox_input_error:
	mutex_destroy(&data->enable_mutex);
	kfree(data);
error:
	return ret;
}

static int __devexit tsl2771_driver_remove(struct i2c_client *client)
{
	struct tsl2771_data *data = i2c_get_clientdata(client);
	int ret = 0;

	if (data->pdata->tsl2771_pwr_control)
		data->pdata->tsl2771_pwr_control(0);

	sysfs_remove_group(&client->dev.kobj, &tsl2771_attr_group);

	if (data->client->irq)
		free_irq(data->client->irq, data);

	if (data->prox_input_dev)
		input_free_device(data->prox_input_dev);

	if (data->als_input_dev)
		input_free_device(data->als_input_dev);

	i2c_set_clientdata(client, NULL);
	mutex_destroy(&data->enable_mutex);
	kfree(data);

	return ret;
}
/* TO DO: Need to run through the power management APIs to make sure we
 * do not break power management and the sensor */
#ifdef CONFIG_PM
static int tsl2771_driver_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tsl2771_data *data = platform_get_drvdata(pdev);

	/* TO DO: May need to retain the interrupt thresholds but won't know
	 * until the thresholds are implemented */
	data->power_context = data->power_state;
	if (data->power_state & 0x2) {
		if (als_prox_debug & 0x4)
			pr_info("%s:Prox was enabled into suspend\n", __func__);
	} else
		tsl2771_prox_enable(data, 0);

	tsl2771_als_enable(data, 0);

	if (!(data->power_state & 0x2)) {
		if (data->pdata->tsl2771_pwr_control)
			data->pdata->tsl2771_pwr_control(0);
	}

	return 0;
}

static int tsl2771_driver_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tsl2771_data *data = platform_get_drvdata(pdev);

	if (data->pdata->tsl2771_pwr_control) {
		data->pdata->tsl2771_pwr_control(1);
		tsl2771_init_device(data);
	}

	if (data->power_context & 0x2) {
		if (als_prox_debug & 0x4)
			pr_info("%s:Prox was enabled into suspend\n", __func__);
	} else
		tsl2771_prox_enable(data, 1);

	if (data->power_context & 0x1) {
		if (als_prox_debug & 0x4)
			pr_info("%s:ALS was enabled\n", __func__);
		tsl2771_als_enable(data, 1);
	}

	return 0;
}

static const struct dev_pm_ops tsl2771_pm_ops = {
	.suspend = tsl2771_driver_suspend,
	.resume = tsl2771_driver_resume,
};
#endif

static const struct i2c_device_id tsl2771_idtable[] = {
	{ TSL2771_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, tsl2771_idtable);

static struct i2c_driver tsl2771_driver = {
	.probe		= tsl2771_driver_probe,
	.remove		= tsl2771_driver_remove,
	.id_table	= tsl2771_idtable,
	.driver = {
		.name = TSL2771_NAME,
#ifdef CONFIG_PM
		.pm = &tsl2771_pm_ops,
#endif
	},
};

static int __init tsl2771_driver_init(void)
{
	return i2c_add_driver(&tsl2771_driver);
}

static void __exit tsl2771_driver_exit(void)
{
	i2c_del_driver(&tsl2771_driver);
}

module_init(tsl2771_driver_init);
module_exit(tsl2771_driver_exit);

MODULE_DESCRIPTION("TSL2771 ALS/Prox Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
