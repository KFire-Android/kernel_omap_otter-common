/*
 * drivers/thermal_framework/governor/case_governor.c
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Sebastien Sabatier <s-sabatier1@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#include <linux/err.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/syscalls.h>

#include <linux/thermal_framework.h>

#include <linux/opp.h>
#include <plat/omap_device.h>

/* System/Case Thermal thresholds */
#define SYS_THRESHOLD_HOT		62000
#define SYS_THRESHOLD_COLD		57000
#define SYS_THRESHOLD_HOT_INC		500
#define INIT_COOLING_LEVEL		0
#define CASE_SUBZONES_NUMBER		4
int case_subzone_number = CASE_SUBZONES_NUMBER;
EXPORT_SYMBOL_GPL(case_subzone_number);

static int sys_threshold_hot = SYS_THRESHOLD_HOT;
static int sys_threshold_cold = SYS_THRESHOLD_COLD;
static int thot = SYS_THRESHOLD_COLD;
static int tcold = SYS_THRESHOLD_COLD;

struct case_governor {
	struct thermal_dev *temp_sensor;
	int cooling_level;
	int max_cooling_level;
};

static struct thermal_dev *therm_fw;
static struct case_governor *case_gov;

static void case_reached_max_state(int temp)
{
	pr_emerg("%s: restart due to thermal case policy (temp == %d)\n",
		 __func__, temp);
	sys_sync();
	kernel_restart(NULL);
}

/**
 * DOC: Introduction
 * =================
 * The SYSTEM Thermal governor maintains the policy for the SYSTEM
 * temperature (PCB and/or case). The main goal of the governor is to
 * get the temperature from a thermal sensor and calculate the current
 * temperature of the case. When the case temperature is above the hot
 * limit, then the SYSTEM Thermal governor will cool the system by
 * throttling CPU frequency.
 * When the temperature is below the cold limit, then the SYSTEM Thermal
 * governor will remove any constraints about CPU frequency.
 * The SYSTEM Thermal governor may use 3 different sensors :
 * - CPU (OMAP) on-die temperature sensor (if there is no PCB sensor)
 * - PCB sensor located closed to CPU die
 * - Dedicated sensor for case temperature management
 * To take into account the response delay between the case temperature
 * and the temperature from one of these sensors, the sensor temperature
 * should be averaged.
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @temp:	Temperature (average on-die CPU temp or PCB temp sensor)
*/

static int case_thermal_manager(struct list_head *cooling_list, int temp)
{
	pr_debug("%s: temp: %d thot: %d level: %d sys_thot: %d sys_tcold: %d",
		 __func__, temp, thot, case_gov->cooling_level,
		 sys_threshold_hot, sys_threshold_cold);

	if (temp < sys_threshold_cold) {
		case_gov->cooling_level = INIT_COOLING_LEVEL;
		/* We want to be notified on the first subzone */
		thot = sys_threshold_cold;
		tcold = sys_threshold_cold;

		pr_debug("%s: temp: %d < sys_threshold_cold, thot: %d:%d (%d)",
			 __func__, temp, thot, tcold, case_gov->cooling_level);

		goto update;
	}

	/* no need to update here */
	if (tcold <= temp && temp <= thot)
		return 0;

	if (temp >= sys_threshold_hot) {
		if (temp < tcold) {
			case_gov->cooling_level--;
			thot = tcold;
			tcold -= SYS_THRESHOLD_HOT_INC;
		} else { /* temp > thot */
			case_gov->cooling_level++;
			tcold = thot;
			thot += SYS_THRESHOLD_HOT_INC;
		}

		pr_debug("%s: temp: %d >= sys_threshold_hot, thot: %d:%d (%d)",
			 __func__, temp, thot, tcold, case_gov->cooling_level);

		if (case_gov->cooling_level >= case_gov->max_cooling_level)
			case_reached_max_state(temp);
	} else if (temp > thot) { /* sys_thot > temp > sys_tcold */
		case_gov->cooling_level++;
		tcold = thot;
		thot = sys_threshold_cold +
			((sys_threshold_hot - sys_threshold_cold) /
			 case_subzone_number) *
			case_gov->cooling_level;

		pr_debug("%s: sys_thot >= temp: %d >= sys_tcold, %d:%d (%d)",
			 __func__, temp, thot, tcold, case_gov->cooling_level);
	} else if (temp < tcold) {
		/* coming from Tj > sys_threshold_hot */
		if (case_gov->cooling_level > case_subzone_number) {
			case_gov->cooling_level = case_subzone_number;
			/*
			 * simplifying computation. from here we won't
			 * change tcold or thot, unless we cross again
			 * sys_threshold_hot.
			 */
			thot = sys_threshold_hot;
			tcold = sys_threshold_cold;

			pr_debug("%s: sys_thot >= temp: %d reseting %d:%d (%d)",
				 __func__, temp, thot, tcold,
				 case_gov->cooling_level);
		}
	}

update:
	thermal_device_call_all(cooling_list, cool_device,
						case_gov->cooling_level);
	thermal_device_call(case_gov->temp_sensor, set_temp_thresh,
								tcold, thot);
	return 0;
}

static int case_process_temp(struct thermal_dev *gov,
				struct list_head *cooling_list,
				struct thermal_dev *temp_sensor,
				int temp)
{
	int ret;

	case_gov->temp_sensor = temp_sensor;
	ret = case_thermal_manager(cooling_list, temp);

	return ret;
}

static int option_get(void *data, u64 *val)
{
	u32 *option = data;

	*val = *option;

	return 0;
}

static int option_set(void *data, u64 val)
{
	u32 *option = data;

	*option = val;

	/* reset intermediate thot & tcold as the sys constraint has changed */
	tcold = sys_threshold_cold;
	thot = sys_threshold_cold;

	thermal_device_call(case_gov->temp_sensor, set_temp_thresh,
				tcold, thot);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(case_fops, option_get, option_set, "%llu\n");

static int case_register_debug_entries(struct thermal_dev *gov,
					struct dentry *d)
{
	(void) debugfs_create_file("case_subzone_number",
			S_IRUGO | S_IWUSR, d, &case_subzone_number,
			&case_fops);
	(void) debugfs_create_file("sys_threshold_hot",
			S_IRUGO | S_IWUSR, d, &sys_threshold_hot,
			&case_fops);
	(void) debugfs_create_file("sys_threshold_cold",
			S_IRUGO | S_IWUSR, d, &sys_threshold_cold,
			&case_fops);
	return 0;
}

static struct thermal_dev_ops case_gov_ops = {
	.process_temp = case_process_temp,
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	.register_debug_entries = case_register_debug_entries,
#endif
};

static int __init case_governor_init(void)
{
	struct thermal_dev *thermal_fw;
	struct device *dev;
	int tmp, opps;

	dev = omap_device_get_by_hwmod_name("mpu");
	if (!dev) {
		pr_err("%s: domain does not know the amount of throttling",
			__func__);
		return -ENODEV;
	}
	opps = opp_get_opp_count(dev);

	dev = omap_device_get_by_hwmod_name("gpu");
	if (!dev) {
		pr_err("%s: domain does not know the amount of throttling",
			__func__);
		return -ENODEV;
	}
	tmp = opp_get_opp_count(dev);
	if (tmp > opps)
		opps = tmp;

	case_gov = kzalloc(sizeof(struct case_governor), GFP_KERNEL);
	if (!case_gov) {
		pr_err("%s:Cannot allocate memory\n", __func__);
		return -ENOMEM;
	}

	thermal_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (thermal_fw) {
		thermal_fw->name = "case_governor";
		thermal_fw->domain_name = "case";
		thermal_fw->dev_ops = &case_gov_ops;
		thermal_governor_dev_register(thermal_fw);
		therm_fw = thermal_fw;
	} else {
		pr_err("%s: Cannot allocate memory\n", __func__);
		kfree(case_gov);
		return -ENOMEM;
	}

	case_gov->cooling_level = INIT_COOLING_LEVEL;
	case_gov->max_cooling_level = CASE_SUBZONES_NUMBER + opps;

	return 0;
}

static void __exit case_governor_exit(void)
{
	thermal_governor_dev_unregister(therm_fw);
	kfree(therm_fw);
	kfree(case_gov);
}

module_init(case_governor_init);
module_exit(case_governor_exit);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("System/Case thermal governor");
MODULE_LICENSE("GPL");
