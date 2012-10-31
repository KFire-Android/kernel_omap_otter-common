/*
 * Case Temperature sensor driver file
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Sebastien Sabatier <s-sabatier1@ti.com>
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/platform_data/case_temperature_sensor.h>

#include <linux/thermal_framework.h>

#define AVERAGE_NUMBER_MAX      200
#define INIT_T_HOT		20000
#define INIT_T_COLD		19000

/*
 * case_temp_sensor structure
 * @pdev - Platform device pointer
 * @dev - device pointer
 * @sensor_mutex - Mutex for sysfs, irq and PM
 */
struct case_temp_sensor {
	struct device *dev;
	struct mutex sensor_mutex;
	struct thermal_dev *therm_fw;
	char domain[10];
	struct delayed_work case_sensor_work;
	int work_delay;
	int avg_is_valid;
	int window_sum;
	int sample_idx;
	int threshold_hot;
	int threshold_cold;
	int hot_event;
	int average_number;
	int sensor_temp_table[AVERAGE_NUMBER_MAX];
};

static int case_read_average_temp(struct case_temp_sensor *temp_sensor,
					const char *domain)
{
	int i;
	int tmp;
	int sensor_temp;

	sensor_temp = thermal_lookup_temp(domain);

	/* if on-die sensor does not report a correct value, then return */
	if (sensor_temp < 0)
		return -EINVAL;

	mutex_lock(&temp_sensor->sensor_mutex);

	if (temp_sensor->sample_idx >= temp_sensor->average_number ||
	    temp_sensor->sample_idx >= AVERAGE_NUMBER_MAX) {
		temp_sensor->avg_is_valid = 1;
		temp_sensor->sample_idx = 0;
	}

	tmp = temp_sensor->sensor_temp_table[temp_sensor->sample_idx];
	temp_sensor->sensor_temp_table[temp_sensor->sample_idx++] = sensor_temp;
	temp_sensor->window_sum += sensor_temp;

	if (temp_sensor->avg_is_valid) {
		temp_sensor->window_sum -= tmp;
		tmp = temp_sensor->average_number;
	} else {
		tmp = temp_sensor->sample_idx;
	}

	for (i = 0; i < temp_sensor->average_number; i++) {
		pr_debug("sensor_temp_table[%d] = %d\n", i,
			temp_sensor->sensor_temp_table[i]);
	}

	mutex_unlock(&temp_sensor->sensor_mutex);

	return temp_sensor->window_sum / tmp;
}

static int case_report_temp(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct case_temp_sensor_pdata *pd = pdev->dev.platform_data;
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);
	int current_temp;

	current_temp = case_read_average_temp(temp_sensor, pd->source_domain);
	temp_sensor->therm_fw->current_temp = current_temp;

	pr_debug("%s: case temp %d valid %d\n", __func__,
		temp_sensor->therm_fw->current_temp,
		temp_sensor->avg_is_valid);
	mutex_lock(&temp_sensor->sensor_mutex);
	if ((temp_sensor->therm_fw->current_temp != -EINVAL) &&
	    (temp_sensor->avg_is_valid == 1)) {
		/* if we are hot, keep sending reports, whatever is hotevent */
		if (current_temp >= temp_sensor->threshold_hot) {
			/* first update the zone, hotevent goes 0 */
			mutex_unlock(&temp_sensor->sensor_mutex);
			thermal_sensor_set_temp(temp_sensor->therm_fw);
			mutex_lock(&temp_sensor->sensor_mutex);
			kobject_uevent(&temp_sensor->dev->kobj, KOBJ_CHANGE);
			/* now hotevent back to 1 until we reach cold thres */
			temp_sensor->hot_event = 1;
		}
		if ((current_temp < temp_sensor->threshold_cold) &&
		    (temp_sensor->hot_event == 1)) {
			mutex_unlock(&temp_sensor->sensor_mutex);
			thermal_sensor_set_temp(temp_sensor->therm_fw);
			mutex_lock(&temp_sensor->sensor_mutex);
			kobject_uevent(&temp_sensor->dev->kobj, KOBJ_CHANGE);
			temp_sensor->hot_event = 0;
		}
	}
	mutex_unlock(&temp_sensor->sensor_mutex);

	return temp_sensor->therm_fw->current_temp;
}

static int case_set_temp_thresh(struct thermal_dev *tdev, int min, int max)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	if (max < min) {
		pr_err("%s:Min is greater then the max\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&temp_sensor->sensor_mutex);
	temp_sensor->threshold_cold = min;
	temp_sensor->threshold_hot = max;
	temp_sensor->hot_event = 0;
	pr_info("%s:threshold_cold %d, threshold_hot %d\n", __func__,
		min, max);
	mutex_unlock(&temp_sensor->sensor_mutex);

	return 0;
}

static int case_report_avg_temp(struct thermal_dev *tdev)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	if (temp_sensor->avg_is_valid)
		return temp_sensor->window_sum / temp_sensor->average_number;
	else
		return temp_sensor->window_sum / temp_sensor->sample_idx;
}

/*
 * debugfs hook functions
 */
static int average_get(void *data, u64 *val)
{
	struct case_temp_sensor *temp_sensor = (struct case_temp_sensor *)data;
	struct platform_device *pdev = to_platform_device(temp_sensor->dev);
	struct case_temp_sensor_pdata *pd = pdev->dev.platform_data;

	*val = case_read_average_temp(temp_sensor, pd->source_domain);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(average_fops, average_get, NULL, "%llu\n");

static int report_delay_get(void *data, u64 *val)
{
	struct case_temp_sensor *temp_sensor = (struct case_temp_sensor *)data;

	*val = temp_sensor->work_delay;

	return 0;
}

static int report_delay_set(void *data, u64 val)
{
	struct case_temp_sensor *temp_sensor = (struct case_temp_sensor *)data;

	mutex_lock(&temp_sensor->sensor_mutex);
	temp_sensor->work_delay = (int)val;
	mutex_unlock(&temp_sensor->sensor_mutex);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(report_delay_fops, report_delay_get,
						report_delay_set, "%llu\n");

static int average_number_get(void *data, u64 *val)
{
	struct case_temp_sensor *temp_sensor = (struct case_temp_sensor *)data;

	*val = temp_sensor->average_number;

	return 0;
}

static int average_number_set(void *data, u64 val)
{
	struct case_temp_sensor *temp_sensor = (struct case_temp_sensor *)data;

	val = clamp((int)val, 1, AVERAGE_NUMBER_MAX);

	mutex_lock(&temp_sensor->sensor_mutex);
	temp_sensor->average_number = (int)val;
	temp_sensor->sample_idx = 0;
	temp_sensor->avg_is_valid = 0;
	temp_sensor->window_sum = 0;
	mutex_unlock(&temp_sensor->sensor_mutex);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(average_number_fops, average_number_get,
						average_number_set, "%llu\n");

static int case_sensor_register_debug_entries(struct thermal_dev *tdev,
					struct dentry *d)
{
	struct platform_device *pdev = to_platform_device(tdev->dev);
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	(void) debugfs_create_file("average_temp",
			S_IRUGO, d, temp_sensor,
			&average_fops);
	(void) debugfs_create_file("report_delay_ms",
			S_IRUGO | S_IWUSR, d, temp_sensor,
			&report_delay_fops);
	(void) debugfs_create_file("average_number",
			S_IRUGO | S_IWUSR, d, temp_sensor,
			&average_number_fops);
	return 0;
}

static struct thermal_dev_ops case_sensor_ops = {
	.set_temp_thresh = case_set_temp_thresh,
	.report_temp = case_report_avg_temp,
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	.register_debug_entries = case_sensor_register_debug_entries,
#endif
};

static void case_sensor_delayed_work_fn(struct work_struct *work)
{
	struct case_temp_sensor *temp_sensor =
				container_of(work, struct case_temp_sensor,
					     case_sensor_work.work);

	case_report_temp(temp_sensor->therm_fw);
	schedule_delayed_work(&temp_sensor->case_sensor_work,
				msecs_to_jiffies(temp_sensor->work_delay));
}

static int __devinit case_temp_sensor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct case_temp_sensor_pdata *pdata = pdev->dev.platform_data;
	struct case_temp_sensor *temp_sensor;
	int ret = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "%s: platform data missing\n", __func__);
		return -EINVAL;
	}

	temp_sensor = kzalloc(sizeof(struct case_temp_sensor), GFP_KERNEL);
	if (!temp_sensor)
		return -ENOMEM;

	/* Init delayed work for Case sensor temperature */
	INIT_DELAYED_WORK(&temp_sensor->case_sensor_work,
			  case_sensor_delayed_work_fn);

	mutex_init(&temp_sensor->sensor_mutex);

	temp_sensor->dev = dev;

	pm_runtime_enable(dev);

	kobject_uevent(&pdev->dev.kobj, KOBJ_ADD);
	platform_set_drvdata(pdev, temp_sensor);

	temp_sensor->therm_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (temp_sensor->therm_fw) {
		temp_sensor->therm_fw->name = "case_sensor";
		temp_sensor->therm_fw->domain_name = "case";
		temp_sensor->therm_fw->dev = temp_sensor->dev;
		temp_sensor->therm_fw->dev_ops = &case_sensor_ops;
		thermal_sensor_dev_register(temp_sensor->therm_fw);
	} else {
		dev_err(&pdev->dev, "%s:Cannot alloc memory for thermal fw\n",
			__func__);
		ret = -ENOMEM;
		goto free_temp_sensor;
	}

	temp_sensor->threshold_hot = INIT_T_HOT;
	temp_sensor->threshold_cold = INIT_T_COLD;
	temp_sensor->hot_event = 0;

	if (pdata->average_number > AVERAGE_NUMBER_MAX)
		temp_sensor->average_number = AVERAGE_NUMBER_MAX;
	else
		temp_sensor->average_number = pdata->average_number;

	temp_sensor->work_delay = pdata->report_delay_ms;
	schedule_work(&temp_sensor->case_sensor_work.work);

	dev_info(&pdev->dev, "%s: Initialised\n", temp_sensor->therm_fw->name);

	return 0;
free_temp_sensor:
	mutex_destroy(&temp_sensor->sensor_mutex);
	kfree(temp_sensor);
	return ret;
}

static int __devexit case_temp_sensor_remove(struct platform_device *pdev)
{
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	thermal_sensor_dev_unregister(temp_sensor->therm_fw);
	kfree(temp_sensor->therm_fw);
	kobject_uevent(&temp_sensor->dev->kobj, KOBJ_REMOVE);
	cancel_delayed_work_sync(&temp_sensor->case_sensor_work);
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&temp_sensor->sensor_mutex);
	kfree(temp_sensor);

	return 0;
}

static void case_temp_sensor_shutdown(struct platform_device *pdev)
{
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&temp_sensor->case_sensor_work);
}

#ifdef CONFIG_PM
static int case_temp_sensor_suspend(struct platform_device *pdev,
				    pm_message_t state)
{
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&temp_sensor->case_sensor_work);

	return 0;
}

static int case_temp_sensor_resume(struct platform_device *pdev)
{
	struct case_temp_sensor *temp_sensor = platform_get_drvdata(pdev);

	schedule_work(&temp_sensor->case_sensor_work.work);

	return 0;
}

#else
case_temp_sensor_suspend NULL
case_temp_sensor_resume NULL

#endif /* CONFIG_PM */

static int case_temp_sensor_runtime_suspend(struct device *dev)
{
	struct case_temp_sensor *temp_sensor =
			platform_get_drvdata(to_platform_device(dev));

	cancel_delayed_work_sync(&temp_sensor->case_sensor_work);

	return 0;
}

static int case_temp_sensor_runtime_resume(struct device *dev)
{
	struct case_temp_sensor *temp_sensor =
			platform_get_drvdata(to_platform_device(dev));

	schedule_work(&temp_sensor->case_sensor_work.work);

	return 0;
}

static const struct dev_pm_ops case_temp_sensor_dev_pm_ops = {
	.runtime_suspend = case_temp_sensor_runtime_suspend,
	.runtime_resume = case_temp_sensor_runtime_resume,
};

static struct platform_driver case_temp_sensor_driver = {
	.probe = case_temp_sensor_probe,
	.remove = __exit_p(case_temp_sensor_remove),
	.suspend = case_temp_sensor_suspend,
	.resume = case_temp_sensor_resume,
	.driver = {
		.owner	= THIS_MODULE,
		.name = "case_temp_sensor",
		.pm = &case_temp_sensor_dev_pm_ops,
	},
	.shutdown = case_temp_sensor_shutdown,
};

static int __init case_temp_sensor_init(void)
{
	return platform_driver_register(&case_temp_sensor_driver);
}

static void __exit case_temp_sensor_exit(void)
{
	platform_driver_unregister(&case_temp_sensor_driver);
}

module_init(case_temp_sensor_init);
module_exit(case_temp_sensor_exit);

MODULE_DESCRIPTION("Case Temperature Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:Case_Temp_Sensor");
MODULE_AUTHOR("Texas Instruments Inc");
