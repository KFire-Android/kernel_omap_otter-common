/*
 * drivers/thermal/omap_die_governor.c
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Dan Murphy <DMurphy@ti.com>
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

#include <linux/thermal_framework.h>

/* CPU Zone information */
#define FATAL_ZONE	5
#define PANIC_ZONE	4
#define ALERT_ZONE	3
#define MONITOR_ZONE	2
#define SAFE_ZONE	1
#define NO_ACTION	0
#define OMAP_FATAL_TEMP 125000
#define OMAP_PANIC_TEMP 110000
#define OMAP_ALERT_TEMP 100000
#define OMAP_MONITOR_TEMP 85000
#define OMAP_SAFE_TEMP  25000


/* TODO: Define this via a configurable file */
#define HYSTERESIS_VALUE 2000
#define NORMAL_TEMP_MONITORING_RATE 1000
#define FAST_TEMP_MONITORING_RATE 250

#define OMAP_GRADIENT_SLOPE 481
#define OMAP_GRADIENT_CONST -12945

/* PCB sensor calculation constants */
#define OMAP_GRADIENT_SLOPE_WITH_PCB 1370
#define OMAP_GRADIENT_CONST_WITH_PCB -635
#define AVERAGE_NUMBER	      20

struct omap_die_governor {
	struct thermal_dev *temp_sensor;
	void (*update_temp_thresh) (struct thermal_dev *, int min, int max);
	int report_rate;
	int panic_zone_reached;
	int cooling_level;
	int hotspot_temp_upper;
	int hotspot_temp_lower;
	int pcb_temp;
	int sensor_temp;
	int absolute_delta;
	int average_period;
	int avg_cpu_sensor_temp;
	int avg_is_valid;
	struct delayed_work average_cpu_sensor_work;
};

static struct thermal_dev *therm_fw;
static struct omap_die_governor *omap_gov;
static struct thermal_dev *pcb_sensor;
static int cpu_sensor_temp_table[AVERAGE_NUMBER];

static LIST_HEAD(cooling_agents);

/**
 * DOC: Introduction
 * =================
 * The OMAP On-Die Temperature governor maintains the policy for the CPU
 * on die temperature sensor.  The main goal of the governor is to receive
 * a temperature report from a thermal sensor and calculate the current
 * thermal zone.  Each zone will sort through a list of cooling agents
 * passed in to determine the correct cooling stategy that will cool the
 * device appropriately for that zone.
 *
 * The temperature that is reported by the temperature sensor is first
 * calculated to an OMAP hot spot temperature.
 * This takes care of the existing temperature gradient between
 * the OMAP hot spot and the on-die temp sensor.
 * Note: The "slope" parameter is multiplied by 1000 in the configuration
 * file to avoid using floating values.
 * Note: The "offset" is defined in milli-celsius degrees.
 *
 * Next the hot spot temperature is then compared to thresholds to determine
 * the proper zone.
 *
 * There are 5 zones identified:
 *
 * FATAL_ZONE: This zone indicates that the on-die temperature has reached
 * a point where the device needs to be rebooted and allow ROM or the bootloader
 * to run to allow the device to cool.
 *
 * PANIC_ZONE: This zone indicates a near fatal temperature is approaching
 * and should impart all neccessary cooling agent to bring the temperature
 * down to an acceptable level.
 *
 * ALERT_ZONE: This zone indicates that die is at a level that may need more
 * agressive cooling agents to keep or lower the temperature.
 *
 * MONITOR_ZONE: This zone is used as a monitoring zone and may or may not use
 * cooling agents to hold the current temperature.
 *
 * SAFE_ZONE: This zone is optimal thermal zone.  It allows the device to
 * run at max levels without imparting any cooling agent strategies.
 *
 * NO_ACTION: Means just that.  There was no action taken based on the current
 * temperature sent in.
*/

/**
 * omap_update_report_rate() - Updates the temperature sensor monitoring rate
 *
 * @new_rate: New measurement rate for the temp sensor
 *
 * No return
 */
static void omap_update_report_rate(struct thermal_dev *temp_sensor,
				int new_rate)
{
	if (omap_gov->report_rate == -EOPNOTSUPP) {
		pr_err("%s:Updating the report rate is not supported\n",
			__func__);
		return;
	}

	if (omap_gov->report_rate != new_rate)
		omap_gov->report_rate =
			thermal_update_temp_rate(temp_sensor, new_rate);
}

/*
 * convert_omap_sensor_temp_to_hotspot_temp() -Convert the temperature from the
 *		OMAP on-die temp sensor into OMAP hot spot temperature.
 *		This takes care of the existing temperature gradient between
 *		the OMAP hot spot and the on-die temp sensor.
 *		When PCB sensor is used, the temperature gradient is computed
 *		from PCB and averaged on-die sensor temperatures.
 *
 * @sensor_temp: Raw temperature reported by the OMAP die temp sensor
 *
 * Returns the calculated hot spot temperature for the zone calculation
 */
static signed int convert_omap_sensor_temp_to_hotspot_temp(int sensor_temp)
{
	int absolute_delta;

	if (pcb_sensor && (omap_gov->avg_is_valid == 1)) {
		omap_gov->pcb_temp = thermal_request_temp(pcb_sensor);
		if (omap_gov->pcb_temp < 0)
			return sensor_temp + omap_gov->absolute_delta;

		absolute_delta = (
			((omap_gov->avg_cpu_sensor_temp - omap_gov->pcb_temp) *
			OMAP_GRADIENT_SLOPE_WITH_PCB / 1000) +
			OMAP_GRADIENT_CONST_WITH_PCB);

		/* Ensure that this formula never returns negative value */
		if (absolute_delta < 0)
			absolute_delta = 0;
	} else {
		absolute_delta = ((sensor_temp * OMAP_GRADIENT_SLOPE / 1000) +
			OMAP_GRADIENT_CONST);
	}

	omap_gov->absolute_delta = absolute_delta;
	pr_debug("%s:sensor %d avg sensor %d pcb %d, delta %d hot spot %d\n",
			__func__, sensor_temp, omap_gov->avg_cpu_sensor_temp,
			omap_gov->pcb_temp, omap_gov->absolute_delta,
			sensor_temp + absolute_delta);

	return sensor_temp + absolute_delta;
}

/*
 * hotspot_temp_to_omap_sensor_temp() - Convert the temperature from
 *		the OMAP hot spot temperature into the OMAP on-die temp sensor.
 *		This is useful to configure the thresholds at OMAP on-die
 *		sensor level. This takes care of the existing temperature
 *		gradient between the OMAP hot spot and the on-die temp sensor.
 *
 * @hot_spot_temp: Hot spot temperature to the be calculated to CPU on-die
 *		temperature value.
 *
 * Returns the calculated cpu on-die temperature.
 */

static signed hotspot_temp_to_sensor_temp(int hot_spot_temp)
{
	if (pcb_sensor && (omap_gov->avg_is_valid == 1))
		return hot_spot_temp - omap_gov->absolute_delta;
	else
		return ((hot_spot_temp - OMAP_GRADIENT_CONST) * 1000) /
			(1000 + OMAP_GRADIENT_SLOPE);
}

/**
 * omap_safe_zone() - THERMAL "Safe Zone" definition:
 *  - No constraint about Max CPU frequency
 *  - No constraint about CPU freq governor
 *  - Normal temperature monitoring rate
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @cpu_temp:	The current adjusted CPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_safe_zone(struct list_head *cooling_list, int cpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int die_temp_upper = 0;
	int die_temp_lower = 0;

	pr_info("%s:hot spot temp %d\n", __func__, cpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		omap_gov->cooling_level = 0;
		thermal_cooling_set_level(&cooling_agents,
					omap_gov->cooling_level);
		list_del_init(&cooling_agents);
		omap_gov->hotspot_temp_lower = OMAP_SAFE_TEMP;
		omap_gov->hotspot_temp_upper = OMAP_MONITOR_TEMP;
		die_temp_lower = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_lower);
		die_temp_upper = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_upper);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			die_temp_lower, die_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			NORMAL_TEMP_MONITORING_RATE);
		if (pcb_sensor)
			omap_gov->average_period = NORMAL_TEMP_MONITORING_RATE;

		omap_gov->panic_zone_reached = 0;
	}

	return 0;
}

/**
 * omap_monitor_zone() - Current device is in a situation that requires
 *			monitoring and may impose agents to keep the device
 *			at a steady state temperature or moderately cool the
 *			device.
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @cpu_temp:	The current adjusted CPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_monitor_zone(struct list_head *cooling_list, int cpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int die_temp_upper = 0;
	int die_temp_lower = 0;

	pr_info("%s:hot spot temp %d\n", __func__, cpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		omap_gov->cooling_level = 0;
		thermal_cooling_set_level(&cooling_agents,
					omap_gov->cooling_level);
		list_del_init(&cooling_agents);
		omap_gov->hotspot_temp_lower =
			(OMAP_MONITOR_TEMP - HYSTERESIS_VALUE);
		omap_gov->hotspot_temp_upper = OMAP_ALERT_TEMP;
		die_temp_lower = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_lower);
		die_temp_upper = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_upper);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			die_temp_lower, die_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			FAST_TEMP_MONITORING_RATE);
		if (pcb_sensor)
			omap_gov->average_period = FAST_TEMP_MONITORING_RATE;

		omap_gov->panic_zone_reached = 0;
	}

	return 0;
}
/**
 * omap_alert_zone() - "Alert Zone" definition:
 *	- If the Panic Zone has never been reached, then
 *	- Define constraint about Max CPU frequency
 *		if Current frequency < Max frequency,
 *		then Max frequency = Current frequency
 *	- Else keep the constraints set previously until
 *		temperature falls to safe zone
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @cpu_temp:	The current adjusted CPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_alert_zone(struct list_head *cooling_list, int cpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int die_temp_upper = 0;
	int die_temp_lower = 0;

	pr_info("%s:hot spot temp %d\n", __func__, cpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		if (omap_gov->panic_zone_reached == 0) {
			/* Temperature rises and enters into alert zone */
			omap_gov->cooling_level = 0;
			thermal_cooling_set_level(&cooling_agents,
						omap_gov->cooling_level);
		}

		list_del_init(&cooling_agents);
		omap_gov->hotspot_temp_lower =
			(OMAP_ALERT_TEMP - HYSTERESIS_VALUE);
		omap_gov->hotspot_temp_upper = OMAP_PANIC_TEMP;
		die_temp_lower = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_lower);
		die_temp_upper = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_upper);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			die_temp_lower, die_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			FAST_TEMP_MONITORING_RATE);
		if (pcb_sensor)
			omap_gov->average_period = FAST_TEMP_MONITORING_RATE;
	}

	return 0;
}

/**
 * omap_panic_zone() - Force CPU frequency to a "safe frequency"
 *     . Force the CPU frequency to a “safe” frequency
 *     . Limit max CPU frequency to the “safe” frequency
 *
 * @cooling_list: The list of cooling devices available to cool the zone
 * @cpu_temp:	The current adjusted CPU temperature
 *
 * Returns 0 on success and -ENODEV for no cooling devices available to cool
 */
static int omap_panic_zone(struct list_head *cooling_list, int cpu_temp)
{
	struct thermal_dev *cooling_dev, *tmp;
	int die_temp_upper = 0;
	int die_temp_lower = 0;

	pr_info("%s:hot spot temp %d\n", __func__, cpu_temp);
	/* TO DO: need to build an algo to find the right cooling agent */
	list_for_each_entry_safe(cooling_dev, tmp, cooling_list, node) {
		if (cooling_dev->dev_ops &&
			cooling_dev->dev_ops->cool_device) {
			/* TO DO: Add cooling agents to a list here */
			list_add(&cooling_dev->node, &cooling_agents);
			goto out;
		} else {
			pr_info("%s:Cannot find cool_device for %s\n",
				__func__, cooling_dev->name);
		}
	}
out:
	if (list_empty(&cooling_agents)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	} else {
		omap_gov->cooling_level++;
		omap_gov->panic_zone_reached++;
		pr_info("%s: Panic zone reached %i times\n",
			__func__, omap_gov->panic_zone_reached);
		thermal_cooling_set_level(&cooling_agents,
					omap_gov->cooling_level);
		list_del_init(&cooling_agents);
		omap_gov->hotspot_temp_lower =
			(OMAP_PANIC_TEMP - HYSTERESIS_VALUE);

		/*
		 * Set the threshold window to below fatal.  This way the
		 * governor can manage the thermal if the temp should rise
		 * while throttling.  We need to be agressive with throttling
		 * should we reach this zone.
		 */
		omap_gov->hotspot_temp_upper = (
			((OMAP_FATAL_TEMP - OMAP_PANIC_TEMP) / 4) *
			omap_gov->panic_zone_reached) + OMAP_PANIC_TEMP;
		if (omap_gov->hotspot_temp_upper >= OMAP_FATAL_TEMP)
			omap_gov->hotspot_temp_upper = OMAP_FATAL_TEMP;

		die_temp_lower = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_lower);
		die_temp_upper = hotspot_temp_to_sensor_temp(
			omap_gov->hotspot_temp_upper);
		thermal_update_temp_thresholds(omap_gov->temp_sensor,
			die_temp_lower, die_temp_upper);
		omap_update_report_rate(omap_gov->temp_sensor,
			FAST_TEMP_MONITORING_RATE);
		if (pcb_sensor)
			omap_gov->average_period = FAST_TEMP_MONITORING_RATE;
	}

	return 0;
}

/**
 * omap_fatal_zone() - Shut-down the system to ensure OMAP Junction
 *			temperature decreases enough
 *
 * @cpu_temp:	The current adjusted CPU temperature
 *
 * No return forces a restart of the system
 */
static void omap_fatal_zone(int cpu_temp)
{
	pr_emerg("%s:FATAL ZONE (hot spot temp: %i)\n", __func__, cpu_temp);

	kernel_restart(NULL);
}

static int omap_cpu_thermal_manager(struct list_head *cooling_list, int temp)
{
	int cpu_temp;

	omap_gov->sensor_temp = temp;
	cpu_temp = convert_omap_sensor_temp_to_hotspot_temp(temp);

	pr_info("%s:sensor %d avg sensor %d pcb %d, delta %d hot spot %d\n",
			__func__, temp, omap_gov->avg_cpu_sensor_temp,
			omap_gov->pcb_temp, omap_gov->absolute_delta,
			cpu_temp);

	if (cpu_temp >= OMAP_FATAL_TEMP) {
		omap_fatal_zone(cpu_temp);
		return FATAL_ZONE;
	} else if (cpu_temp >= OMAP_PANIC_TEMP) {
		omap_panic_zone(cooling_list, cpu_temp);
		return PANIC_ZONE;
	} else if (cpu_temp < (OMAP_PANIC_TEMP - HYSTERESIS_VALUE)) {
		if (cpu_temp >= OMAP_ALERT_TEMP) {
			omap_alert_zone(cooling_list, cpu_temp);
			return ALERT_ZONE;
		} else if (cpu_temp < (OMAP_ALERT_TEMP - HYSTERESIS_VALUE)) {
			if (cpu_temp >= OMAP_MONITOR_TEMP) {
				omap_monitor_zone(cooling_list, cpu_temp);
				return MONITOR_ZONE;
			} else {
				/*
				 * this includes the case where :
				 * (OMAP_MONITOR_TEMP - HYSTERESIS_VALUE) <= T
				 * && T < OMAP_MONITOR_TEMP
				 */
				omap_safe_zone(cooling_list, cpu_temp);
				return SAFE_ZONE;
			}
		} else {
			/*
			 * this includes the case where :
			 * (OMAP_ALERT_TEMP - HYSTERESIS_VALUE) <= T
			 * && T < OMAP_ALERT_TEMP
			 */
			omap_monitor_zone(cooling_list, cpu_temp);
			return MONITOR_ZONE;
		}
	} else {
		/*
		 * this includes the case where :
		 * (OMAP_PANIC_TEMP - HYSTERESIS_VALUE) <= T < OMAP_PANIC_TEMP
		 */
		omap_alert_zone(cooling_list, cpu_temp);
		return ALERT_ZONE;
	}

	return NO_ACTION;
}

/*
 * Make an average of the OMAP on-die temperature
 * this is helpful to handle burst activity of OMAP when extrapolating
 * the OMAP hot spot temperature from on-die sensor and PCB temperature
 * Re-evaluate the temperature gradient between hot spot and on-die sensor
 * (See absolute_delta) and reconfigure the thresholds if needed
 */
static void average_on_die_temperature(void)
{
	int i;
	int die_temp_lower = 0;
	int die_temp_upper = 0;

	if (omap_gov->temp_sensor == NULL)
		return;

	/* Read current temperature */
	omap_gov->sensor_temp = thermal_request_temp(omap_gov->temp_sensor);

	/* if on-die sensor does not report a correct value, then return */
	if (omap_gov->sensor_temp == -EINVAL)
		return;

	/* Update historical buffer */
	for (i = 1; i < AVERAGE_NUMBER; i++) {
		cpu_sensor_temp_table[AVERAGE_NUMBER - i] =
		cpu_sensor_temp_table[AVERAGE_NUMBER - i - 1];
	}
	cpu_sensor_temp_table[0] = omap_gov->sensor_temp;

	if (cpu_sensor_temp_table[AVERAGE_NUMBER - 1] == 0)
		omap_gov->avg_is_valid = 0;
	else
		omap_gov->avg_is_valid = 1;

	/* Compute the new average value */
	omap_gov->avg_cpu_sensor_temp = 0;
	for (i = 0; i < AVERAGE_NUMBER; i++)
		omap_gov->avg_cpu_sensor_temp += cpu_sensor_temp_table[i];

	omap_gov->avg_cpu_sensor_temp =
		(omap_gov->avg_cpu_sensor_temp / AVERAGE_NUMBER);

	/*
	 * Reconfigure the current temperature thresholds according
	 * to the current PCB temperature
	 */
	convert_omap_sensor_temp_to_hotspot_temp(omap_gov->sensor_temp);
	die_temp_lower = hotspot_temp_to_sensor_temp(
		omap_gov->hotspot_temp_lower);
	die_temp_upper = hotspot_temp_to_sensor_temp(
		omap_gov->hotspot_temp_upper);
	thermal_update_temp_thresholds(omap_gov->temp_sensor,
		die_temp_lower, die_temp_upper);

	return;
}

static void average_cpu_sensor_delayed_work_fn(struct work_struct *work)
{
	struct omap_die_governor *omap_gov =
				container_of(work, struct omap_die_governor,
					     average_cpu_sensor_work.work);

	average_on_die_temperature();

	schedule_delayed_work(&omap_gov->average_cpu_sensor_work,
				msecs_to_jiffies(omap_gov->average_period));
}

static int omap_process_cpu_temp(struct list_head *cooling_list,
				struct thermal_dev *temp_sensor,
				int temp)
{
	if (!strcmp(temp_sensor->name, "pcb_sensor")) {
		if (pcb_sensor == NULL) {
			pr_info("%s: Setting %s pointer\n",
				__func__, temp_sensor->name);
			pcb_sensor = temp_sensor;
		}
		omap_gov->pcb_temp = temp;
		return 0;
	}

	omap_gov->temp_sensor = temp_sensor;
	return omap_cpu_thermal_manager(cooling_list, temp);
}

static struct thermal_dev_ops omap_gov_ops = {
	.process_temp = omap_process_cpu_temp,
};

static int __init omap_die_governor_init(void)
{
	struct thermal_dev *thermal_fw;

	omap_gov = kzalloc(sizeof(struct omap_die_governor), GFP_KERNEL);
	if (!omap_gov) {
		pr_err("%s:Cannot allocate memory\n", __func__);
		return -ENOMEM;
	}

	thermal_fw = kzalloc(sizeof(struct thermal_dev), GFP_KERNEL);
	if (thermal_fw) {
		thermal_fw->name = "omap_ondie_governor";
		thermal_fw->domain_name = "cpu";
		thermal_fw->dev_ops = &omap_gov_ops;
		thermal_governor_dev_register(thermal_fw);
		therm_fw = thermal_fw;
	} else {
		pr_err("%s: Cannot allocate memory\n", __func__);
		kfree(omap_gov);
		return -ENOMEM;
	}

	pcb_sensor = NULL;

	/* Init delayed work to average on-die temperature */
	INIT_DELAYED_WORK(&omap_gov->average_cpu_sensor_work,
			  average_cpu_sensor_delayed_work_fn);

	omap_gov->average_period = NORMAL_TEMP_MONITORING_RATE;
	omap_gov->avg_is_valid = 0;
	schedule_delayed_work(&omap_gov->average_cpu_sensor_work,
			msecs_to_jiffies(0));
	return 0;
}

static void __exit omap_die_governor_exit(void)
{
	cancel_delayed_work_sync(&omap_gov->average_cpu_sensor_work);
	thermal_governor_dev_unregister(therm_fw);
	kfree(therm_fw);
	kfree(omap_gov);
}

module_init(omap_die_governor_init);
module_exit(omap_die_governor_exit);

MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
MODULE_DESCRIPTION("OMAP on-die thermal governor");
MODULE_LICENSE("GPL");
