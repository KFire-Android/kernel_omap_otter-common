/*
 * cma3000_d0x.c
 * VTI CMA3000_D0x Accelerometer driver
 *	Supports I2C/SPI interfaces
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

#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/i2c/cma3000.h>

#include "cma3000_d0x.h"

#define CMA3000_WHOAMI      0x00
#define CMA3000_REVID       0x01
#define CMA3000_CTRL        0x02
#define CMA3000_STATUS      0x03
#define CMA3000_RSTR        0x04
#define CMA3000_INTSTATUS   0x05
#define CMA3000_DOUTX       0x06
#define CMA3000_DOUTY       0x07
#define CMA3000_DOUTZ       0x08
#define CMA3000_MDTHR       0x09
#define CMA3000_MDFFTMR     0x0A
#define CMA3000_FFTHR       0x0B

#define CMA3000_RANGE2G    (1 << 7)
#define CMA3000_RANGE8G    (0 << 7)
#define CMA3000_BUSI2C     (0 << 4)
#define CMA3000_MODEMASK   (7 << 1)
#define CMA3000_GRANGEMASK (1 << 7)

#define CMA3000_STATUS_PERR    1
#define CMA3000_INTSTATUS_FFDET (1 << 2)

/* Settling time delay in ms */
#define CMA3000_SETDELAY    30

/* Delay for clearing interrupt in us */
#define CMA3000_INTDELAY    44


/*
 * Bit weights in mg for bit 0, other bits need
 * multipy factor 2^n. Eight bit is the sign bit.
 */
#define BIT_TO_2G  18
#define BIT_TO_8G  71

/*
 * Conversion for each of the eight modes to g, depending
 * on G range i.e 2G or 8G. Some modes always operate in
 * 8G.
 */

static int mode_to_mg[8][2] = {
	{0, 0},
	{BIT_TO_8G, BIT_TO_2G},
	{BIT_TO_8G, BIT_TO_2G},
	{BIT_TO_8G, BIT_TO_8G},
	{BIT_TO_8G, BIT_TO_8G},
	{BIT_TO_8G, BIT_TO_2G},
	{BIT_TO_8G, BIT_TO_2G},
	{0, 0},
};

/* interval between samples for the different rates, in msecs */
static const unsigned int cma3000_measure_interval[] = {
	0, 1000 / 100, 1000 / 400, 1000 / 40,
};

static uint32_t accl_debug;
module_param_named(cma3000_debug, accl_debug, uint, 0664);

static int cma3000_set_mode(struct cma3000_accl_data *data, int val)
{
	uint8_t ctrl;
	int error = 0;

	mutex_lock(&data->mutex);
	val &= (CMA3000_MODEMASK >> 1);
	ctrl = cma3000_read(data, CMA3000_CTRL, "ctrl");
	if (ctrl < 0) {
		error = ctrl;
		goto err_op2_failed;
	}

	ctrl &= ~CMA3000_MODEMASK;
	ctrl |= (val << 1);

	if (!data->client->irq)
		ctrl |= 0x01;

	error = cma3000_set(data, CMA3000_CTRL, ctrl, "ctrl");
	if (error < 0)
		goto err_op1_failed;

	/* Settling time delay required after mode change */
	msleep(CMA3000_SETDELAY);

err_op1_failed:
err_op2_failed:
	mutex_unlock(&data->mutex);
	return error;

}

static ssize_t cma3000_show_attr_mode(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	uint8_t mode;
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);

	mode = cma3000_read(data, CMA3000_CTRL, "ctrl");
	if (mode < 0)
		return mode;

	return sprintf(buf, "%d\n", (mode & CMA3000_MODEMASK) >> 1);
}

static ssize_t cma3000_store_attr_mode(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	if (val < CMAMODE_DEFAULT || val > CMAMODE_POFF)
		return -EINVAL;

	return cma3000_set_mode(data, val);

}

static ssize_t cma3000_show_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", data->req_poll_rate);
}

static ssize_t cma3000_store_attr_delay(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);
	unsigned long interval;
	int error;
	int i = 0;

	error = strict_strtoul(buf, 0, &interval);
	if (error)
		return error;

	if (interval < 0)
		return -EINVAL;

	cancel_delayed_work_sync(&data->input_work);

	if (interval == 0) {
		/* Set to the fastest speed */
		i = CMAMODE_MEAS400;
	} else {
		if (interval < cma3000_measure_interval[CMAMODE_MEAS40])
			i = CMAMODE_MEAS400;
		else if (interval >= cma3000_measure_interval[CMAMODE_MEAS100])
			i = CMAMODE_MEAS100;
		else
			i = CMAMODE_MEAS40;
	}

	data->req_poll_rate = interval;
	cma3000_set_mode(data, i);
	schedule_delayed_work(&data->input_work, 0);

	return count;

}

static ssize_t cma3000_show_attr_enable(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	uint8_t mode;
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);

	mode = cma3000_read(data, CMA3000_CTRL, "ctrl");
	if (mode < 0)
		return mode;

	return sprintf(buf, "%d\n", (mode & CMA3000_MODEMASK) >> 1);
}

static ssize_t cma3000_store_attr_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error, enable;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	if (val < CMAMODE_DEFAULT || val > CMAMODE_POFF)
		return -EINVAL;

	if (val == CMAMODE_DEFAULT || val == CMAMODE_POFF)
		enable = val;
	else
		enable = data->pdata.mode;

	cma3000_set_mode(data, enable);

	if (val == CMAMODE_DEFAULT || val == CMAMODE_POFF)
		cancel_delayed_work_sync(&data->input_work);
	else
		schedule_delayed_work(&data->input_work, 0);

	return count;
}

static ssize_t cma3000_show_attr_grange(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	uint8_t mode;
	int g_range;

	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);

	mode = cma3000_read(data, CMA3000_CTRL, "ctrl");
	if (mode < 0)
		return mode;

	g_range = (mode & CMA3000_GRANGEMASK) ? CMARANGE_2G : CMARANGE_8G;
	return sprintf(buf, "%d\n", g_range);
}

static ssize_t cma3000_store_attr_grange(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error, g_range, fuzz_x, fuzz_y, fuzz_z;
	uint8_t ctrl;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		goto err_op3_failed;

	mutex_lock(&data->mutex);
	ctrl = cma3000_read(data, CMA3000_CTRL, "ctrl");
	if (ctrl < 0) {
		error = ctrl;
		goto err_op2_failed;
	}

	ctrl &= ~CMA3000_GRANGEMASK;

	if (val == CMARANGE_2G) {
		ctrl |= CMA3000_RANGE2G;
		data->pdata.g_range = CMARANGE_2G;
	} else if (val == CMARANGE_8G) {
		ctrl |= CMA3000_RANGE8G;
		data->pdata.g_range = CMARANGE_8G;
	} else {
		error = -EINVAL;
		goto err_op2_failed;
	}

	g_range = data->pdata.g_range;
	fuzz_x = data->pdata.fuzz_x;
	fuzz_y = data->pdata.fuzz_y;
	fuzz_z = data->pdata.fuzz_z;

	disable_irq(data->client->irq);
	error = cma3000_set(data, CMA3000_CTRL, ctrl, "ctrl");
	if (error < 0)
		goto err_op1_failed;

	input_set_abs_params(data->input_dev, ABS_X, -g_range,
				g_range, fuzz_x, 0);
	input_set_abs_params(data->input_dev, ABS_Y, -g_range,
				g_range, fuzz_y, 0);
	input_set_abs_params(data->input_dev, ABS_Z, -g_range,
				g_range, fuzz_z, 0);

	enable_irq(data->client->irq);
	mutex_unlock(&data->mutex);
	return count;

err_op1_failed:
	enable_irq(data->client->irq);
err_op2_failed:
	mutex_unlock(&data->mutex);
err_op3_failed:
	return error;
}

static ssize_t cma3000_show_attr_mdthr(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	uint8_t mode;
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);

	mode = cma3000_read(data, CMA3000_MDTHR, "mdthr");
	if (mode < 0)
		return mode;

	return sprintf(buf, "%d\n", mode);
}

static ssize_t cma3000_store_attr_mdthr(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	data->pdata.mdthr = val;
	disable_irq(data->client->irq);
	error = cma3000_set(data, CMA3000_MDTHR, val, "mdthr");
	enable_irq(data->client->irq);
	mutex_unlock(&data->mutex);

	/* If there was error during write, return error */
	if (error < 0)
		return error;
	else
		return count;
}

static ssize_t cma3000_show_attr_mdfftmr(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t mode;

	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);

	mode = cma3000_read(data, CMA3000_MDFFTMR, "mdfftmr");
	if (mode < 0)
		return mode;

	return sprintf(buf, "%d\n", mode);
}

static ssize_t cma3000_store_attr_mdfftmr(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	data->pdata.mdfftmr = val;
	disable_irq(data->client->irq);
	error = cma3000_set(data, CMA3000_MDFFTMR, val, "mdthr");
	enable_irq(data->client->irq);
	mutex_unlock(&data->mutex);

	/* If there was error during write, return error */
	if (error < 0)
		return error;
	else
		return count;
}

static ssize_t cma3000_show_attr_ffthr(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	uint8_t mode;

	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);

	mode = cma3000_read(data, CMA3000_FFTHR, "ffthr");
	if (mode < 0)
		return mode;

	return sprintf(buf, "%d\n", mode);
}

static ssize_t cma3000_store_attr_ffthr(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cma3000_accl_data *data = platform_get_drvdata(pdev);
	unsigned long val;
	int error;

	error = strict_strtoul(buf, 0, &val);
	if (error)
		return error;

	mutex_lock(&data->mutex);
	data->pdata.ffthr = val;
	disable_irq(data->client->irq);
	error = cma3000_set(data, CMA3000_FFTHR, val, "mdthr");
	enable_irq(data->client->irq);
	mutex_unlock(&data->mutex);

	/* If there was error during write, return error */
	if (error < 0)
		return error;
	else
		return count;
}

static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO,
		cma3000_show_attr_mode, cma3000_store_attr_mode);

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO,
		cma3000_show_attr_enable, cma3000_store_attr_enable);

static DEVICE_ATTR(delay, S_IWUSR | S_IRUGO,
		cma3000_show_attr_delay, cma3000_store_attr_delay);

static DEVICE_ATTR(grange, S_IWUSR | S_IRUGO,
		cma3000_show_attr_grange, cma3000_store_attr_grange);

static DEVICE_ATTR(mdthr, S_IWUSR | S_IRUGO,
		cma3000_show_attr_mdthr, cma3000_store_attr_mdthr);

static DEVICE_ATTR(mdfftmr, S_IWUSR | S_IRUGO,
		cma3000_show_attr_mdfftmr, cma3000_store_attr_mdfftmr);

static DEVICE_ATTR(ffthr, S_IWUSR | S_IRUGO,
		cma3000_show_attr_ffthr, cma3000_store_attr_ffthr);


static struct attribute *cma_attrs[] = {
	&dev_attr_mode.attr,
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_grange.attr,
	&dev_attr_mdthr.attr,
	&dev_attr_mdfftmr.attr,
	&dev_attr_ffthr.attr,
	NULL,
};

static struct attribute_group cma3000_attr_group = {
	.attrs = cma_attrs,
};

static void decode_mg(struct cma3000_accl_data *data, int *datax,
				int *datay, int *dataz)
{
	/* Data in 2's complement, convert to mg */
	*datax = (((s8)(*datax)) * (data->bit_to_mg));
	*datay = (((s8)(*datay)) * (data->bit_to_mg));
	*dataz = (((s8)(*dataz)) * (data->bit_to_mg));
}

static void cma3000_read_report_data(struct cma3000_accl_data *data)
{
	int  datax, datay, dataz;
	u8 ctrl, mode, range, intr_status;

	mutex_lock(&data->mutex);
	intr_status = cma3000_read(data, CMA3000_INTSTATUS, "interrupt status");
	if (intr_status < 0) {
		pr_info("%s:No interrupt from the device\n", __func__);
		goto not_from_device;
	}

	/* Check if free fall is detected, report immediately */
	if (intr_status & CMA3000_INTSTATUS_FFDET) {
		if (accl_debug)
			pr_info("%s:Free fall\n", __func__);
		input_report_abs(data->input_dev, ABS_MISC, 1);
		input_sync(data->input_dev);
	} else {
		input_report_abs(data->input_dev, ABS_MISC, 0);
	}

	datax = cma3000_read(data, CMA3000_DOUTX, "X");
	datay = cma3000_read(data, CMA3000_DOUTY, "Y");
	dataz = cma3000_read(data, CMA3000_DOUTZ, "Z");

	if (accl_debug)
		pr_info("%s:Raw x= %d, y= %d, z= %d\n",
			__func__, datax, datay, dataz);

	ctrl = cma3000_read(data, CMA3000_CTRL, "ctrl");
	mode = (ctrl & CMA3000_MODEMASK) >> 1;
	range = (ctrl & CMA3000_GRANGEMASK) >> 7;

	data->bit_to_mg = mode_to_mg[mode][range];

	/* Interrupt not for this device */
	if (data->bit_to_mg == 0)
		goto not_from_device;

	/* Decode register values to milli g */
	decode_mg(data, &datax, &datay, &dataz);
	if (accl_debug)
		pr_info("%s:Reporting x= %d, y= %d, z= %d\n",
			__func__, datax, datay, dataz);
	input_report_abs(data->input_dev, ABS_X, datax);
	input_report_abs(data->input_dev, ABS_Y, datay);
	input_report_abs(data->input_dev, ABS_Z, dataz);
	input_sync(data->input_dev);

not_from_device:
	mutex_unlock(&data->mutex);
	if (data->client->irq)
		enable_irq(data->client->irq);
}

static void cma3000_input_work_func(struct work_struct *work)
{
	struct cma3000_accl_data *cma = container_of((struct delayed_work *)work,
						  struct cma3000_accl_data,
						  input_work);

	cma3000_read_report_data(cma);
	if (!cma->client->irq)
		schedule_delayed_work(&cma->input_work,
				msecs_to_jiffies(cma->req_poll_rate));
}

static irqreturn_t cma3000_isr(int irq, void *dev_id)
{
	struct cma3000_accl_data *cma = dev_id;

	disable_irq_nosync(irq);
	schedule_delayed_work(&cma->input_work, 0);

	return IRQ_HANDLED;
}

static int cma3000_reset(struct cma3000_accl_data *data)
{
	int ret;

	/* Reset sequence */
	cma3000_set(data, CMA3000_RSTR, 0x02, "Reset");
	cma3000_set(data, CMA3000_RSTR, 0x0A, "Reset");
	cma3000_set(data, CMA3000_RSTR, 0x04, "Reset");

	/* Settling time delay */
	mdelay(10);

	ret = cma3000_read(data, CMA3000_STATUS, "Status");
	if (ret < 0) {
		dev_err(&data->client->dev, "Reset failed\n");
		return ret;
	} else if (ret & CMA3000_STATUS_PERR) {
		dev_err(&data->client->dev, "Parity Error\n");
		return -EIO;
	} else {
		return 0;
	}
}

int cma3000_poweron(struct cma3000_accl_data *data)
{
	uint8_t ctrl = 0, mdthr, mdfftmr, ffthr, mode;
	int g_range, ret;

	g_range = data->pdata.g_range;
	mode = data->pdata.mode;
	mdthr = data->pdata.mdthr;
	mdfftmr = data->pdata.mdfftmr;
	ffthr = data->pdata.ffthr;

	if (mode < CMAMODE_DEFAULT || mode > CMAMODE_POFF) {
		data->pdata.mode = CMAMODE_MOTDET;
		mode = data->pdata.mode;
		dev_info(&data->client->dev,
			"Invalid mode specified, assuming Motion Detect\n");
	}

	if (g_range == CMARANGE_2G) {
		ctrl = (mode << 1) | CMA3000_RANGE2G;
	} else if (g_range == CMARANGE_8G) {
		ctrl = (mode << 1) | CMA3000_RANGE8G;
	} else {
		dev_info(&data->client->dev,
			"Invalid G range specified, assuming 8G\n");
		ctrl = (mode << 1) | CMA3000_RANGE8G;
		data->pdata.g_range = CMARANGE_8G;
	}
#ifdef CONFIG_INPUT_CMA3000_I2C
	ctrl |= CMA3000_BUSI2C;
#endif
	/* Disable IEQ if not configured for irq mode*/
	if (!data->client->irq)
		ctrl |= 0x1;


	cma3000_set(data, CMA3000_MDTHR, mdthr, "Motion Detect Threshold");
	cma3000_set(data, CMA3000_MDFFTMR, mdfftmr, "Time register");
	cma3000_set(data, CMA3000_FFTHR, ffthr, "Free fall threshold");
	ret = cma3000_set(data, CMA3000_CTRL, ctrl, "Mode setting");
	if (ret < 0)
		return -EIO;

	msleep(CMA3000_SETDELAY);

	return 0;
}

int cma3000_poweroff(struct cma3000_accl_data *data)
{
	int ret;

	ret = cma3000_set_mode(data, CMAMODE_POFF);
	msleep(CMA3000_SETDELAY);

	return ret;
}

int cma3000_init(struct cma3000_accl_data *data)
{
	int ret = 0, fuzz_x, fuzz_y, fuzz_z, g_range;
	uint32_t irqflags;
	uint8_t ctrl;

	INIT_DELAYED_WORK(&data->input_work, cma3000_input_work_func);

	if (data->client->dev.platform_data == NULL) {
		dev_err(&data->client->dev, "platform data not found\n");
		goto err_op2_failed;
	}

	memcpy(&(data->pdata), data->client->dev.platform_data,
		sizeof(struct cma3000_platform_data));

	ret = cma3000_reset(data);
	if (ret)
		goto err_op2_failed;

	ret = cma3000_read(data, CMA3000_REVID, "Revid");
	if (ret < 0)
		goto err_op2_failed;

	pr_info("CMA3000 Acclerometer : Revision %x\n", ret);

	/* Bring it out of default power down state */
	ret = cma3000_poweron(data);
	if (ret < 0)
		goto err_op2_failed;

	data->req_poll_rate = data->pdata.def_poll_rate;
	fuzz_x = data->pdata.fuzz_x;
	fuzz_y = data->pdata.fuzz_y;
	fuzz_z = data->pdata.fuzz_z;
	g_range = data->pdata.g_range;
	irqflags = data->pdata.irqflags;

	data->input_dev = input_allocate_device();
	if (data->input_dev == NULL) {
		ret = -ENOMEM;
		dev_err(&data->client->dev,
			"Failed to allocate input device\n");
		goto err_op2_failed;
	}

	data->input_dev->name = "cma3000-acclerometer";

#ifdef CONFIG_INPUT_CMA3000_I2C
	data->input_dev->id.bustype = BUS_I2C;
#endif

	 __set_bit(EV_ABS, data->input_dev->evbit);
	 __set_bit(EV_MSC, data->input_dev->evbit);

	input_set_abs_params(data->input_dev, ABS_X, -g_range,
				g_range, fuzz_x, 0);
	input_set_abs_params(data->input_dev, ABS_Y, -g_range,
				g_range, fuzz_y, 0);
	input_set_abs_params(data->input_dev, ABS_Z, -g_range,
				g_range, fuzz_z, 0);
	input_set_abs_params(data->input_dev, ABS_MISC, 0,
				1, 0, 0);

	ret = input_register_device(data->input_dev);
	if (ret) {
		dev_err(&data->client->dev,
			"Unable to register input device\n");
		goto err_op2_failed;
	}

	mutex_init(&data->mutex);

	if (data->client->irq) {
		ret = request_irq(data->client->irq, cma3000_isr,
					irqflags | IRQF_ONESHOT,
					data->client->name, data);

		if (ret < 0) {
			dev_err(&data->client->dev,
				"request_threaded_irq failed\n");
			goto err_op1_failed;
		}
	} else {
		/*There is no IRQ set, disable IRQ on CMA*/
		ctrl = cma3000_read(data, CMA3000_CTRL, "Status");
		ctrl |= 0x1;
		cma3000_set(data, CMA3000_CTRL, ctrl, "Disable IRQ");
	}

	ret = sysfs_create_group(&data->client->dev.kobj, &cma3000_attr_group);
	if (ret) {
		dev_err(&data->client->dev,
			"failed to create sysfs entries\n");
		goto err_op1_failed;
	}

	cma3000_set_mode(data, CMAMODE_POFF);

	return 0;

err_op1_failed:
	mutex_destroy(&data->mutex);
	input_unregister_device(data->input_dev);
err_op2_failed:
	if (data != NULL) {
		if (data->input_dev != NULL)
			input_free_device(data->input_dev);
	}

	return ret;
}

int cma3000_exit(struct cma3000_accl_data *data)
{
	int ret;

	ret = cma3000_poweroff(data);

	if (data->client->irq)
		free_irq(data->client->irq, data);

	mutex_destroy(&data->mutex);
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	sysfs_remove_group(&data->client->dev.kobj, &cma3000_attr_group);
	return ret;
}
