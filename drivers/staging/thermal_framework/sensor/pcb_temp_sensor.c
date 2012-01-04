/*
 * PCB Temperature sensor driver file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
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

#include <linux/err.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/stddef.h>
#include <linux/sysfs.h>
#include <linux/thermal_framework.h>
#include <linux/types.h>
#include <linux/mutex.h>

#include <plat/common.h>
#include <plat/pcb_temperature_sensor.h>
#include <linux/i2c/twl6030-gpadc.h>

#define PCB_REPORT_DELAY_MS	1000

/*
 * pcb_temp_sensor structure
 * @pdev - Platform device pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for sysfs, irq and PM
 */
struct pcb_temp_sensor {
	struct platform_device *pdev;
	struct device *dev;
	struct mutex sensor_mutex;
	struct spinlock lock;
	struct thermal_dev *therm_fw;
	struct delayed_work pcb_sensor_work;
	int work_delay;
	int debug_temp;
};

#define TWL6030_ADC_START_VALUE 126
#define TWL6030_ADC_END_VALUE   978
#define TWL6030_GPADC_CHANNEL     4
/*
 * Temperature values in milli degrees celsius ADC code values from 978 to 126
 */
static int adc_to_temp[] = {
	-40000, -40000, -40000, -40000, -40000, -37000, -35000, -33000, -31000,
	-29000, -28000, -27000, -25000, -24000, -23000, -22000, -21000, -20000,
	-19000, -18000, -17000, -17000, -16000, -15000, -14000, -14000, -13000,
	-12000, -12000, -11000, -11000, -10000, -10000, -9000, -8000, -8000,
	-7000, -7000, -6000, -6000, -6000, -5000, -5000, -4000, -4000, -3000,
	-3000, -3000, -2000, -2000, -1000, -1000, -1000, 0, 0, 0, 1000, 1000,
	1000, 2000, 2000, 2000, 3000, 3000, 3000, 4000, 4000, 4000, 5000, 5000,
	5000, 6000, 6000, 6000, 6000, 7000, 7000, 7000, 8000, 8000, 8000, 8000,
	9000, 9000, 9000, 9000, 10000, 10000, 10000, 10000, 11000, 11000,
	11000, 11000, 12000, 12000, 12000, 12000, 12000, 13000, 13000, 13000,
	13000, 14000, 14000, 14000, 14000, 14000, 15000, 15000, 15000, 15000,
	15000, 16000, 16000, 16000, 16000, 16000, 17000, 17000, 17000, 17000,
	17000, 18000, 18000, 18000, 18000, 18000, 19000, 19000, 19000, 19000,
	19000, 20000, 20000, 20000, 20000, 20000, 20000, 21000, 21000, 21000,
	21000, 21000, 22000, 22000, 22000, 22000, 22000, 22000, 23000, 23000,
	23000, 23000, 23000, 23000, 24000, 24000, 24000, 24000, 24000, 24000,
	25000, 25000, 25000, 25000, 25000, 25000, 25000, 26000, 26000, 26000,
	26000, 26000, 26000, 27000, 27000, 27000, 27000, 27000, 27000, 27000,
	28000, 28000, 28000, 28000, 28000, 28000, 29000, 29000, 29000, 29000,
	29000, 29000, 29000, 30000, 30000, 30000, 30000, 30000, 30000, 30000,
	31000, 31000, 31000, 31000, 31000, 31000, 31000, 32000, 32000, 32000,
	32000, 32000, 32000, 32000, 33000, 33000, 33000, 33000, 33000, 33000,
	33000, 33000, 34000, 34000, 34000, 34000, 34000, 34000, 34000, 35000,
	35000, 35000, 35000, 35000, 35000, 35000, 35000, 36000, 36000, 36000,
	36000, 36000, 36000, 36000, 36000, 37000, 37000, 37000, 37000, 37000,
	37000, 37000, 38000, 38000, 38000, 38000, 38000, 38000, 38000, 38000,
	39000, 39000, 39000, 39000, 39000, 39000, 39000, 39000, 39000, 40000,
	40000, 40000, 40000, 40000, 40000, 40000, 40000, 41000, 41000, 41000,
	41000, 41000, 41000, 41000, 41000, 42000, 42000, 42000, 42000, 42000,
	42000, 42000, 42000, 43000, 43000, 43000, 43000, 43000, 43000, 43000,
	43000, 43000, 44000, 44000, 44000, 44000, 44000, 44000, 44000, 44000,
	45000, 45000, 45000, 45000, 45000, 45000, 45000, 45000, 45000, 46000,
	46000, 46000, 46000, 46000, 46000, 46000, 46000, 47000, 47000, 47000,
	47000, 47000, 47000, 47000, 47000, 47000, 48000, 48000, 48000, 48000,
	48000, 48000, 48000, 48000, 48000, 49000, 49000, 49000, 49000, 49000,
	49000, 49000, 49000, 49000, 50000, 50000, 50000, 50000, 50000, 50000,
	50000, 50000, 51000, 51000, 51000, 51000, 51000, 51000, 51000, 51000,
	51000, 52000, 52000, 52000, 52000, 52000, 52000, 52000, 52000, 52000,
	53000, 53000, 53000, 53000, 53000, 53000, 53000, 53000, 53000, 54000,
	54000, 54000, 54000, 54000, 54000, 54000, 54000, 54000, 55000, 55000,
	55000, 55000, 55000, 55000, 55000, 55000, 55000, 56000, 56000, 56000,
	56000, 56000, 56000, 56000, 56000, 56000, 57000, 57000, 57000, 57000,
	57000, 57000, 57000, 57000, 57000, 58000, 58000, 58000, 58000, 58000,
	58000, 58000, 58000, 58000, 59000, 59000, 59000, 59000, 59000, 59000,
	59000, 59000, 59000, 60000, 60000, 60000, 60000, 60000, 60000, 60000,
	60000, 61000, 61000, 61000, 61000, 61000, 61000, 61000, 61000, 61000,
	62000, 62000, 62000, 62000, 62000, 62000, 62000, 62000, 62000, 63000,
	63000, 63000, 63000, 63000, 63000, 63000, 63000, 63000, 64000, 64000,
	64000, 64000, 64000, 64000, 64000, 64000, 64000, 65000, 65000, 65000,
	65000, 65000, 65000, 65000, 65000, 66000, 66000, 66000, 66000, 66000,
	66000, 66000, 66000, 66000, 67000, 67000, 67000, 67000, 67000, 67000,
	67000, 67000, 68000, 68000, 68000, 68000, 68000, 68000, 68000, 68000,
	68000, 69000, 69000, 69000, 69000, 69000, 69000, 69000, 69000, 70000,
	70000, 70000, 70000, 70000, 70000, 70000, 70000, 70000, 71000, 71000,
	71000, 71000, 71000, 71000, 71000, 71000, 72000, 72000, 72000, 72000,
	72000, 72000, 72000, 72000, 73000, 73000, 73000, 73000, 73000, 73000,
	73000, 73000, 74000, 74000, 74000, 74000, 74000, 74000, 74000, 74000,
	75000, 75000, 75000, 75000, 75000, 75000, 75000, 75000, 76000, 76000,
	76000, 76000, 76000, 76000, 76000, 76000, 77000, 77000, 77000, 77000,
	77000, 77000, 77000, 77000, 78000, 78000, 78000, 78000, 78000, 78000,
	78000, 79000, 79000, 79000, 79000, 79000, 79000, 79000, 79000, 80000,
	80000, 80000, 80000, 80000, 80000, 80000, 81000, 81000, 81000, 81000,
	81000, 81000, 81000, 82000, 82000, 82000, 82000, 82000, 82000, 82000,
	82000, 83000, 83000, 83000, 83000, 83000, 83000, 83000, 84000, 84000,
	84000, 84000, 84000, 84000, 84000, 85000, 85000, 85000, 85000, 85000,
	85000, 85000, 86000, 86000, 86000, 86000, 86000, 86000, 87000, 87000,
	87000, 87000, 87000, 87000, 87000, 88000, 88000, 88000, 88000, 88000,
	88000, 88000, 89000, 89000, 89000, 89000, 89000, 89000, 90000, 90000,
	90000, 90000, 90000, 90000, 91000, 91000, 91000, 91000, 91000, 91000,
	91000, 92000, 92000, 92000, 92000, 92000, 92000, 93000, 93000, 93000,
	93000, 93000, 93000, 94000, 94000, 94000, 94000, 94000, 94000, 95000,
	95000, 95000, 95000, 95000, 96000, 96000, 96000, 96000, 96000, 96000,
	97000, 97000, 97000, 97000, 97000, 97000, 98000, 98000, 98000, 98000,
	98000, 99000, 99000, 99000, 99000, 99000, 100000, 100000, 100000,
	100000, 100000, 100000, 101000, 101000, 101000, 101000, 101000, 102000,
	102000, 102000, 102000, 102000, 103000, 103000, 103000, 103000, 103000,
	104000, 104000, 104000, 104000, 104000, 105000, 105000, 105000, 105000,
	106000, 106000, 106000, 106000, 106000, 107000, 107000, 107000, 107000,
	108000, 108000, 108000, 108000, 108000, 109000, 109000, 109000, 109000,
	110000, 110000, 110000, 110000, 111000, 111000, 111000, 111000, 112000,
	112000, 112000, 112000, 112000, 113000, 113000, 113000, 114000, 114000,
	114000, 114000, 115000, 115000, 115000, 115000, 116000, 116000, 116000,
	116000, 117000, 117000, 117000, 118000, 118000, 118000, 118000, 119000,
	119000, 119000, 120000, 120000, 120000, 120000, 121000, 121000, 121000,
	122000, 122000, 122000, 123000, 123000, 123000, 124000, 124000, 124000,
	124000, 125000, 125000, 125000, 125000, 125000, 125000, 125000, 125000,
	125000, 125000, 125000, 125000,
};

static int adc_to_temp_conversion(int adc_val)
{
	if ((adc_val < TWL6030_ADC_START_VALUE) ||
		(adc_val > TWL6030_ADC_END_VALUE)) {
		pr_err("%s:Temp read is invalid %i\n", __func__, adc_val);
		return -EINVAL;
	}

	return adc_to_temp[TWL6030_ADC_END_VALUE - adc_val];
}

static int pcb_read_current_temp(struct pcb_temp_sensor *temp_sensor)
{
	int temp = 0;
	struct twl6030_gpadc_request req;
	int val;

	req.channels = (1 << TWL6030_GPADC_CHANNEL);
	req.method = TWL6030_GPADC_SW2;
	req.func_cb = NULL;
	val = twl6030_gpadc_conversion(&req);
	if (val < 0) {
		pr_err("%s:TWL6030_GPADC conversion is invalid %d\n",
			__func__, val);
		return -EINVAL;
	}
	temp = adc_to_temp_conversion(req.buf[TWL6030_GPADC_CHANNEL].code);

	return temp;
}

static int pcb_get_temp(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct pcb_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	temp_sensor->therm_fw->current_temp =
			pcb_read_current_temp(temp_sensor);

	return temp_sensor->therm_fw->current_temp;
}

static void pcb_report_fw_temp(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct pcb_temp_sensor *temp_sensor = platform_get_drvdata(pdev);
	int ret;

	pcb_read_current_temp(temp_sensor);
	if (temp_sensor->therm_fw->current_temp != -EINVAL) {
		ret = thermal_sensor_set_temp(temp_sensor->therm_fw);
		if (ret == -ENODEV)
			pr_err("%s:thermal_sensor_set_temp reports error\n",
				__func__);
		else
			cancel_delayed_work_sync(&temp_sensor->pcb_sensor_work);
		kobject_uevent(&temp_sensor->dev->kobj, KOBJ_CHANGE);
	}
}

/*
 * sysfs hook functions
 */
static ssize_t show_temp_user_space(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pcb_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	return sprintf(buf, "%d\n", temp_sensor->debug_temp);
}

static ssize_t set_temp_user_space(struct device *dev,
			struct device_attribute *devattr,
			const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pcb_temp_sensor *temp_sensor = platform_get_drvdata(pdev);
	long val;

	if (strict_strtol(buf, 10, &val)) {
		count = -EINVAL;
		goto out;
	}

	/* Set new temperature */
	temp_sensor->debug_temp = val;

	temp_sensor->therm_fw->current_temp = val;
	thermal_sensor_set_temp(temp_sensor->therm_fw);
	/* Send a kobj_change */
	kobject_uevent(&temp_sensor->dev->kobj, KOBJ_CHANGE);

out:
	return count;
}

static int pcb_temp_sensor_read_temp(struct device *dev,
				      struct device_attribute *devattr,
				      char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pcb_temp_sensor *temp_sensor = platform_get_drvdata(pdev);
	int temp = 0;

	temp = pcb_read_current_temp(temp_sensor);

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR(debug_user, S_IWUSR | S_IRUGO, show_temp_user_space,
			  set_temp_user_space);
static DEVICE_ATTR(temp1_input, S_IRUGO, pcb_temp_sensor_read_temp,
			  NULL);

static struct attribute *pcb_temp_sensor_attributes[] = {
	&dev_attr_temp1_input.attr,
	&dev_attr_debug_user.attr,
	NULL
};

static const struct attribute_group pcb_temp_sensor_group = {
	.attrs = pcb_temp_sensor_attributes,
};

static struct thermal_dev_ops pcb_sensor_ops = {
	.report_temp = pcb_get_temp,
};

static void pcb_sensor_delayed_work_fn(struct work_struct *work)
{
	struct pcb_temp_sensor *temp_sensor =
				container_of(work, struct pcb_temp_sensor,
					     pcb_sensor_work.work);

	pcb_report_fw_temp(temp_sensor->therm_fw);
	schedule_delayed_work(&temp_sensor->pcb_sensor_work,
				msecs_to_jiffies(temp_sensor->work_delay));
}

static int __devinit pcb_temp_sensor_probe(struct platform_device *pdev)
{
	struct pcb_temp_sensor_pdata *pdata = pdev->dev.platform_data;
	struct pcb_temp_sensor *temp_sensor;
	int ret = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "%s: platform data missing\n", __func__);
		return -EINVAL;
	}

	temp_sensor = kzalloc(sizeof(struct pcb_temp_sensor), GFP_KERNEL);
	if (!temp_sensor)
		return -ENOMEM;

	/* Init delayed work for PCB sensor temperature */
	INIT_DELAYED_WORK(&temp_sensor->pcb_sensor_work,
			  pcb_sensor_delayed_work_fn);

	spin_lock_init(&temp_sensor->lock);
	mutex_init(&temp_sensor->sensor_mutex);

	temp_sensor->pdev = pdev;
	temp_sensor->dev = &pdev->dev;

	kobject_uevent(&pdev->dev.kobj, KOBJ_ADD);
	platform_set_drvdata(pdev, temp_sensor);

	temp_sensor->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (temp_sensor->therm_fw) {
		temp_sensor->therm_fw->name = "pcb_sensor";
		temp_sensor->therm_fw->domain_name = "cpu";
		temp_sensor->therm_fw->dev = temp_sensor->dev;
		temp_sensor->therm_fw->dev_ops = &pcb_sensor_ops;
		thermal_sensor_dev_register(temp_sensor->therm_fw);
	} else {
		dev_err(&pdev->dev, "%s:Cannot alloc memory for thermal fw\n",
			__func__);
		ret = -ENOMEM;
		goto therm_fw_alloc_err;
	}

	ret = sysfs_create_group(&pdev->dev.kobj,
				 &pcb_temp_sensor_group);
	if (ret) {
		dev_err(&pdev->dev, "could not create sysfs files\n");
		goto sysfs_create_err;
	}

	temp_sensor->work_delay = PCB_REPORT_DELAY_MS;
	schedule_delayed_work(&temp_sensor->pcb_sensor_work,
			msecs_to_jiffies(0));

	dev_info(&pdev->dev, "%s : '%s'\n", temp_sensor->therm_fw->name,
			pdata->name);

	return 0;

sysfs_create_err:
	thermal_sensor_dev_unregister(temp_sensor->therm_fw);
	kfree(temp_sensor->therm_fw);
	platform_set_drvdata(pdev, NULL);
therm_fw_alloc_err:
	mutex_destroy(&temp_sensor->sensor_mutex);
	kfree(temp_sensor);
	return ret;
}

static int __devexit pcb_temp_sensor_remove(struct platform_device *pdev)
{
	struct pcb_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &pcb_temp_sensor_group);
	cancel_delayed_work_sync(&temp_sensor->pcb_sensor_work);
	thermal_sensor_dev_unregister(temp_sensor->therm_fw);
	kfree(temp_sensor->therm_fw);
	kobject_uevent(&temp_sensor->dev->kobj, KOBJ_REMOVE);
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&temp_sensor->sensor_mutex);
	kfree(temp_sensor);

	return 0;
}

static int pcb_temp_sensor_runtime_suspend(struct device *dev)
{
	return 0;
}

static int pcb_temp_sensor_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops pcb_temp_sensor_dev_pm_ops = {
	.runtime_suspend = pcb_temp_sensor_runtime_suspend,
	.runtime_resume = pcb_temp_sensor_runtime_resume,
};

static struct platform_driver pcb_temp_sensor_driver = {
	.probe = pcb_temp_sensor_probe,
	.remove = pcb_temp_sensor_remove,
	.driver = {
		.name = "pcb_temp_sensor",
		.pm = &pcb_temp_sensor_dev_pm_ops,
	},
};

int __init pcb_temp_sensor_init(void)
{
	if (!cpu_is_omap446x())
		return 0;

	return platform_driver_register(&pcb_temp_sensor_driver);
}

static void __exit pcb_temp_sensor_exit(void)
{
	platform_driver_unregister(&pcb_temp_sensor_driver);
}

module_init(pcb_temp_sensor_init);
module_exit(pcb_temp_sensor_exit);

MODULE_DESCRIPTION("PCB Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRIVER_NAME);
MODULE_AUTHOR("Texas Instruments Inc");
