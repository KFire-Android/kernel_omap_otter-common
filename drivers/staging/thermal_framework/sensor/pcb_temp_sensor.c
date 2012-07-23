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
 * NTC termistor (NCP15WB473F) schematic connection for OMAP4460 board:
 *
 *	     [Vref]
 *	       |
 *	       $ (Rpu)
 *	       |
 *	       +----+-----------[Vin]
 *	       |    |
 *	      [Rt]  $ (Rpd)
 *	       |    |
 *	      -------- (ground)
 *
 * NTC termistor resistanse (Rt, k) calculated from following formula:
 *
 * Rt = Rpd * Rpu * Vin / (Rpd * (Vref - Vin) - Rpu * Vin)
 *
 * where	Vref (GPADC_VREF4) - reference voltage, Vref = 1250 mV;
 *			Vin (GPADC_IN4) - measuring voltage, Vin = 0...1250 mV;
 *			Rpu (R1041) - pullup resistor, Rpu = 10 k;
 *			Rpd (R1043) - pulldown resistor, Rpd = 220 k;
 *
 * Pcb temp sensor temperature (t, C) calculated from following formula:
 *
 * t = 1 / (ln(Rt / Rt0) / B + 1 / T0) - 273
 *
 * where	Rt0 - NTC termistor resistance at 25 C, Rt0 = 47 k;
 *			B - specific constant, B = 4131 K;
 *			T0 - temperature, T0 = 298 K
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
	int debug_temp;
};

#define TWL603X_MVOLT_START_VALUE	156
#define TWL603X_MVOLT_END_VALUE		1191
#define TWL6030_GPADC_CHANNEL		4
/*
 * Temperature values in degrees celsius,
 * voltage values from 156 to 1191 milli volts
 */
static s8 mvolt_to_temp[] = {
		125, 125, 125, 124, 124, 124, 124, 123, 123, 123, 122, 122,
		122, 122, 121, 121, 121, 121, 120, 120, 120, 120, 119, 119,
		119, 119, 118, 118, 118, 118, 117, 117, 117, 117, 117, 116,
		116, 116, 116, 115, 115, 115, 115, 115, 114, 114, 114, 114,
		113, 113, 113, 113, 113, 112, 112, 112, 112, 112, 111, 111,
		111, 111, 111, 110, 110, 110, 110, 110, 109, 109, 109, 109,
		109, 108, 108, 108, 108, 108, 107, 107, 107, 107, 107, 107,
		106, 106, 106, 106, 106, 105, 105, 105, 105, 105, 105, 104,
		104, 104, 104, 104, 104, 103, 103, 103, 103, 103, 103, 102,
		102, 102, 102, 102, 102, 101, 101, 101, 101, 101, 101, 100,
		100, 100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 98, 98,
		98, 98, 98, 98, 98, 97, 97, 97, 97, 97, 97, 96, 96, 96, 96,
		96, 96, 96, 95, 95, 95, 95, 95, 95, 95, 94, 94, 94, 94, 94,
		94, 94, 93, 93, 93, 93, 93, 93, 93, 93, 92, 92, 92, 92, 92,
		92, 92, 91, 91, 91, 91, 91, 91, 91, 91, 90, 90, 90, 90, 90,
		90, 90, 89, 89, 89, 89, 89, 89, 89, 89, 88, 88, 88, 88, 88,
		88, 88, 88, 87, 87, 87, 87, 87, 87, 87, 87, 86, 86, 86, 86,
		86, 86, 86, 86, 86, 85, 85, 85, 85, 85, 85, 85, 85, 84, 84,
		84, 84, 84, 84, 84, 84, 84, 83, 83, 83, 83, 83, 83, 83, 83,
		83, 82, 82, 82, 82, 82, 82, 82, 82, 81, 81, 81, 81, 81, 81,
		81, 81, 81, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 79, 79,
		79, 79, 79, 79, 79, 79, 79, 78, 78, 78, 78, 78, 78, 78, 78,
		78, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 76, 76, 76, 76,
		76, 76, 76, 76, 76, 76, 75, 75, 75, 75, 75, 75, 75, 75, 75,
		74, 74, 74, 74, 74, 74, 74, 74, 74, 74, 73, 73, 73, 73, 73,
		73, 73, 73, 73, 73, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
		71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 71, 70, 70, 70, 70,
		70, 70, 70, 70, 70, 70, 69, 69, 69, 69, 69, 69, 69, 69, 69,
		69, 69, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 67, 67, 67,
		67, 67, 67, 67, 67, 67, 67, 67, 66, 66, 66, 66, 66, 66, 66,
		66, 66, 66, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 65, 64,
		64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 63, 63, 63, 63, 63,
		63, 63, 63, 63, 63, 63, 62, 62, 62, 62, 62, 62, 62, 62, 62,
		62, 62, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 61, 60, 60,
		60, 60, 60, 60, 60, 60, 60, 60, 60, 59, 59, 59, 59, 59, 59,
		59, 59, 59, 59, 59, 58, 58, 58, 58, 58, 58, 58, 58, 58, 58,
		58, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 56, 56,
		56, 56, 56, 56, 56, 56, 56, 56, 56, 55, 55, 55, 55, 55, 55,
		55, 55, 55, 55, 55, 54, 54, 54, 54, 54, 54, 54, 54, 54, 54,
		54, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 53, 52, 52, 52,
		52, 52, 52, 52, 52, 52, 52, 52, 51, 51, 51, 51, 51, 51, 51,
		51, 51, 51, 51, 51, 50, 50, 50, 50, 50, 50, 50, 50, 50, 50,
		50, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48,
		48, 48, 48, 48, 48, 48, 48, 48, 47, 47, 47, 47, 47, 47, 47,
		47, 47, 47, 47, 46, 46, 46, 46, 46, 46, 46, 46, 46, 46, 45,
		45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 44, 44,
		44, 44, 44, 44, 44, 44, 43, 43, 43, 43, 43, 43, 43, 43, 43,
		43, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 41, 41, 41,
		41, 41, 41, 41, 41, 41, 41, 40, 40, 40, 40, 40, 40, 40, 40,
		40, 40, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 38, 38,
		38, 38, 38, 38, 38, 38, 38, 38, 37, 37, 37, 37, 37, 37, 37,
		37, 37, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 35, 35, 35,
		35, 35, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 34, 34, 34,
		34, 33, 33, 33, 33, 33, 33, 33, 33, 33, 32, 32, 32, 32, 32,
		32, 32, 32, 32, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30,
		30, 30, 30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 29, 29, 29,
		28, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27, 27,
		27, 27, 26, 26, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25,
		25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 23, 23, 23, 23, 23,
		23, 23, 23, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21,
		21, 21, 20, 20, 20, 20, 20, 20, 20, 19, 19, 19, 19, 19, 19,
		19, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 17, 16,
		16, 16, 16, 16, 16, 15, 15, 15, 15, 15, 15, 14, 14, 14, 14,
		14, 13, 13, 13, 13, 13, 13, 12, 12, 12, 12, 12, 11, 11, 11,
		11, 11, 10, 10, 10, 10, 10, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 7,
		7, 7, 7, 6, 6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3,
		2, 2, 2, 1, 1, 1, 1, 0, 0, 0, -1, -1, -1, -2, -2, -2, -2, -3,
		-3, -4, -4, -4, -5, -5, -5, -6, -6, -6, -7, -7, -8, -8, -8,
		-9, -9, -10, -10, -11, -11, -12, -12, -13, -13, -14, -14, -15,
		-16, -16, -17, -18, -18, -19, -20, -21, -21, -22, -23, -24,
		-25, -26, -27, -28, -30, -31, -33, -34, -36, -39, -40,
};

static int mvolt_to_temp_conversion(int mvolt)
{
	if ((mvolt < TWL603X_MVOLT_START_VALUE) ||
		(mvolt > TWL603X_MVOLT_END_VALUE)) {
		pr_err("%s:Temp read is invalid %i\n", __func__, mvolt);
		return -EINVAL;
	}

	return mvolt_to_temp[mvolt - TWL603X_MVOLT_START_VALUE] * 1000;
}

static int pcb_read_current_temp(struct pcb_temp_sensor *temp_sensor)
{
	int temp = 0;
	struct twl6030_gpadc_request req;
	int val;

	req.channels = (1 << TWL6030_GPADC_CHANNEL);
	req.method = TWL6030_GPADC_SW2;
	req.func_cb = NULL;
	req.type = TWL6030_GPADC_WAIT;
	val = twl6030_gpadc_conversion(&req);
	if (val < 0) {
		pr_err("%s:TWL6030_GPADC conversion is invalid %d\n",
			__func__, val);
		return -EINVAL;
	}
	temp = mvolt_to_temp_conversion(req.rbuf[TWL6030_GPADC_CHANNEL]);

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

	spin_lock_init(&temp_sensor->lock);
	mutex_init(&temp_sensor->sensor_mutex);

	temp_sensor->pdev = pdev;
	temp_sensor->dev = &pdev->dev;

	kobject_uevent(&pdev->dev.kobj, KOBJ_ADD);
	platform_set_drvdata(pdev, temp_sensor);

	temp_sensor->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (temp_sensor->therm_fw) {
		temp_sensor->therm_fw->name = "pcb_sensor";
		temp_sensor->therm_fw->domain_name = "pcb";
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
