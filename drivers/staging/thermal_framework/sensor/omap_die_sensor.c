/*
 * OMAP Bandgap temperature sensor adaptation to TI Thermal Framework
 *
 * Copyright (C) 2011-2012 Texas Instruments Inc.
 * Contact:
 *	Eduardo Valentin <eduardo.valentin@ti.com>
 *	Radhesh Fadnis <radhesh.fadnis@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/thermal_framework.h>

#include "omap-bandgap.h"

#define MAX_SENSOR_NAME 50

struct omap_thermal_data {
	struct thermal_dev therm_fw;
	struct omap_bandgap *bg_ptr;
	struct work_struct report_temperature_work;
	struct mutex thermal_mutex; /* to synchronize PM ops */
	struct notifier_block pm_notifier;
	bool enabled;
};

static void report_temperature_delayed_work_fn(struct work_struct *work)
{
	int ret, temp;
	struct omap_thermal_data *therm_data;

	therm_data = container_of(work, struct omap_thermal_data,
						report_temperature_work);

	ret = omap_bandgap_read_temperature(therm_data->bg_ptr,
					therm_data->therm_fw.sen_id, &temp);
	if (ret < 0)
		return;

	therm_data->therm_fw.current_temp = temp;
	ret = thermal_sensor_set_temp(&therm_data->therm_fw);
	/*
	 * Currently there are no governor/cooling devices identified for
	 * Core domain. This results in repetitive error messages on console,
	 * whenever there is an temperature report update.
	 * Once the governor/cooling devices are identified/added, need to make
	 * the debug statement as dev_err.
	 */
	if (ret)
		pr_err_once("%s: Fail to set temp\n", __func__);

	return;
}

static int omap_bandgap_report_temp(struct thermal_dev *tdev)
{
	int temp, ret;
	struct omap_thermal_data *therm_data;

	if (tdev == NULL) {
		pr_err("%s:Not a valid device\n", __func__);
		return -ENODEV;
	}

	therm_data = container_of(tdev, struct omap_thermal_data, therm_fw);

	ret = omap_bandgap_read_temperature(therm_data->bg_ptr, tdev->sen_id,
								&temp);
	if (ret < 0) {
		dev_err(therm_data->bg_ptr->dev,
			"Failed to read Bandgap temp\n");
		return ret;
	}

	therm_data->therm_fw.current_temp = temp;

	return therm_data->therm_fw.current_temp;
}

static
int omap_bandgap_set_temp_thresh(struct thermal_dev *tdev,
				int new_tcold, int new_thot)
{
	int ret, t_cold, t_hot;
	struct omap_thermal_data *therm_data;

	if (tdev == NULL) {
		pr_err("%s:Not a valid device\n", __func__);
		return -ENODEV;
	}

	therm_data = container_of(tdev, struct omap_thermal_data, therm_fw);

	if (new_tcold >= new_thot) {
		dev_err(therm_data->bg_ptr->dev,
			"Invalid new thresholds t_cold:%d t_hot:%d\n",
					new_tcold, new_thot);
		return -EINVAL;
	}

	ret = omap_bandgap_read_tcold(therm_data->bg_ptr, tdev->sen_id,
								&t_cold);
	if (ret) {
		dev_err(therm_data->bg_ptr->dev,
			"Failed to read cold threshold val\n");
		return ret;
	}

	ret = omap_bandgap_read_thot(therm_data->bg_ptr, tdev->sen_id, &t_hot);
	if (ret) {
		dev_err(therm_data->bg_ptr->dev,
			"Failed to read hot threshold val\n");
		return ret;
	}

	/*
	 * When setting the NEW thresholds, it is necessary to decide which
	 * threshold (t_cold or t_hot) to configure first. If not, for a SMALL
	 * PERIOD of time, till BOTH threshold are configured, there is a
	 * probability that, the t_cold value may be grater than the t_hot
	 * value (vice versa) in the bandgap registers. The below logic
	 * make sure that t_cold and t_hot thresholds are configured properly.
	 * For example: If the configured t_cold = 80 and t_hot = 90 and new
	 * thresholds are new_tcold = 60 and new_thot = 70. Since new_thot (70)
	 * is less than old t_cold (80), first change the cold threshold,
	 * otherwise t_hot value on the sensor will be lesser than t_cold,
	 * which is not correct. Similar logic applies when the thresholds are
	 * going upwards.
	 */
	if (new_thot < t_cold) {
		ret = omap_bandgap_write_tcold(therm_data->bg_ptr,
						tdev->sen_id, new_tcold);
		ret |= omap_bandgap_write_thot(therm_data->bg_ptr,
						tdev->sen_id, new_thot);
	} else {
		ret |= omap_bandgap_write_thot(therm_data->bg_ptr,
						tdev->sen_id, new_thot);
		ret |= omap_bandgap_write_tcold(therm_data->bg_ptr,
						tdev->sen_id, new_tcold);
	}

	return ret;
}

static int omap_bandgap_set_measuring_rate(struct thermal_dev *tdev, int rate)
{
	struct omap_thermal_data *therm_data;

	if (tdev == NULL) {
		pr_err("%s:Not a valid device\n", __func__);
		return -ENODEV;
	}

	therm_data = container_of(tdev, struct omap_thermal_data, therm_fw);

	omap_bandgap_write_update_interval(therm_data->bg_ptr, tdev->sen_id,
					   rate);

	return rate;
}

static
int omap_bandgap_report_slope(struct thermal_dev *tdev, const char *rel_name)
{
	struct omap_thermal_data *therm_data;

	if (tdev == NULL) {
		pr_err("%s:Not a valid device\n", __func__);
		return -ENODEV;
	}

	therm_data = container_of(tdev, struct omap_thermal_data, therm_fw);

	return therm_data->therm_fw.slope;
}

static
int omap_bandgap_report_offset(struct thermal_dev *tdev, const char *rel_name)
{
	struct omap_thermal_data *therm_data;

	if (tdev == NULL) {
		pr_err("%s:Not a valid device\n", __func__);
		return -ENODEV;
	}

	therm_data = container_of(tdev, struct omap_thermal_data, therm_fw);

	return therm_data->therm_fw.constant;
}

static struct thermal_dev_ops omap_sensor_ops = {
	.report_temp = omap_bandgap_report_temp,
	.set_temp_thresh = omap_bandgap_set_temp_thresh,
	.set_temp_report_rate = omap_bandgap_set_measuring_rate,
	.init_slope = omap_bandgap_report_slope,
	.init_offset = omap_bandgap_report_offset
};

int omap_thermal_report_temperature(struct omap_bandgap *bg_ptr, int id)
{
	struct omap_thermal_data *therm_data;

	if (bg_ptr == NULL) {
		pr_err("%s:Not a valid device\n", __func__);
		return -ENODEV;
	}

	therm_data = omap_bandgap_get_sensor_data(bg_ptr, id);
	if (therm_data == NULL) {
		dev_err(bg_ptr->dev,
			"%s:Not a valid data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&therm_data->thermal_mutex);

	if (therm_data->enabled)
		schedule_work(&therm_data->report_temperature_work);

	mutex_unlock(&therm_data->thermal_mutex);

	return 0;
}

int omap_thermal_remove_sensor(struct omap_bandgap *bg_ptr, int id)
{
	struct omap_thermal_data *therm_data;

	if (bg_ptr == NULL) {
		pr_err("%s:Not a valid device\n", __func__);
		return -ENODEV;
	}

	therm_data = omap_bandgap_get_sensor_data(bg_ptr, id);
	if (therm_data == NULL) {
		dev_err(bg_ptr->dev, "%s:Not a valid data\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&therm_data->thermal_mutex);

	therm_data->enabled = false;
	cancel_work_sync(&therm_data->report_temperature_work);
	thermal_sensor_dev_unregister(&therm_data->therm_fw);

	mutex_unlock(&therm_data->thermal_mutex);

	unregister_pm_notifier(&therm_data->pm_notifier);

	return 0;
}

static int omap_thermal_pm_notifier_cb(struct notifier_block *notifier,
				unsigned long pm_event,  void *unused)
{
	struct omap_thermal_data *therm_data = container_of(notifier, struct
							    omap_thermal_data,
							    pm_notifier);

	mutex_lock(&therm_data->thermal_mutex);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		therm_data->enabled = false;
		cancel_work_sync(&therm_data->report_temperature_work);
		break;
	case PM_POST_SUSPEND:
		therm_data->enabled = true;
		break;
	}

	mutex_unlock(&therm_data->thermal_mutex);

	return NOTIFY_DONE;
}

static struct notifier_block thermal_die_pm_notifier = {
	.notifier_call = omap_thermal_pm_notifier_cb,
};

int omap_thermal_expose_sensor(struct omap_bandgap *bg_ptr, int id,
				char *domain)
{
	int ret = 0;
	struct omap_thermal_data *data;
	char *sensor_name;

	if ((bg_ptr == NULL) || (domain == NULL)) {
		pr_err("%s:Not a valid device\n", __func__);
		return -EINVAL;
	}

	data = devm_kzalloc(bg_ptr->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(bg_ptr->dev, "Bandgap data - kzalloc fail\n");
		return -ENOMEM;
	}

	sensor_name = devm_kzalloc(bg_ptr->dev, MAX_SENSOR_NAME, GFP_KERNEL);
	if (!sensor_name) {
		dev_err(bg_ptr->dev, "Bandgap data - kzalloc fail\n");
		return -ENOMEM;
	}

	data->bg_ptr = bg_ptr;

	/* Init deferred work to report temperature */
	INIT_WORK(&data->report_temperature_work,
		  report_temperature_delayed_work_fn);

	data->therm_fw.dev_ops = devm_kzalloc(bg_ptr->dev,
					sizeof(*data->therm_fw.dev_ops),
					GFP_KERNEL);
	if (!data->therm_fw.dev_ops) {
		dev_err(bg_ptr->dev, "Bandgap data - kzalloc fail\n");
		return -ENOMEM;
	}

	mutex_init(&data->thermal_mutex);

	data->pm_notifier = thermal_die_pm_notifier;
	if (register_pm_notifier(&data->pm_notifier))
		dev_err(bg_ptr->dev, "PM registration failed!\n");

	/* Construct the sensor name for the domain */
	sprintf(sensor_name, "omap_%s_sensor", domain);
	data->therm_fw.name = sensor_name;

	data->therm_fw.domain_name =
		bg_ptr->conf->sensors[id].domain;
	data->therm_fw.dev = data->bg_ptr->dev;
	*data->therm_fw.dev_ops = omap_sensor_ops;
	data->therm_fw.sen_id = id;
	data->therm_fw.slope = data->bg_ptr->conf->sensors[id].slope;
	data->therm_fw.constant =
		data->bg_ptr->conf->sensors[id].constant;

	data->enabled = true;

	ret = omap_bandgap_set_sensor_data(data->bg_ptr, id, data);
	if (ret) {
		dev_err(bg_ptr->dev, "Fail to store TFW data\n");
		return ret;
	}

	ret = thermal_sensor_dev_register(&data->therm_fw);
	if (ret) {
		dev_err(bg_ptr->dev, "Fail to register to TFW\n");
		return ret;
	}

	if (data->bg_ptr->conf->sensors[id].ts_data->stats_en) {
		thermal_init_stats(&data->therm_fw,
		data->bg_ptr->conf->sensors[id].ts_data->avg_period,
		data->bg_ptr->conf->sensors[id].ts_data->avg_number,
		data->bg_ptr->conf->sensors[id].ts_data->safe_temp_trend);
	} else {
		return 0;
	}

	if (bg_ptr->conf->sensors[id].ts_data->stats_en) {
		domain = bg_ptr->conf->sensors[id].domain;
		if (thermal_enable_avg(domain))
			pr_warn("thermal_enable_avg for domain %s failed\n",
				domain);
		if (thermal_enable_trend(domain))
			pr_warn("thermal_enable_trend for domain %s failed\n",
			domain);
	}

	return 0;
}
