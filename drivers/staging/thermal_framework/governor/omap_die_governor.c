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
#include <linux/suspend.h>
#include <linux/thermal_framework.h>
#include <plat/tmp102_temp_sensor.h>
#include <plat/cpu.h>

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
#define DECREASE_MPU_FREQ_PERIOD 2000

#define OMAP_GRADIENT_SLOPE_4460    348
#define OMAP_GRADIENT_CONST_4460  -9301
#define OMAP_GRADIENT_SLOPE_4470    308
#define OMAP_GRADIENT_CONST_4470  -7896

/* PCB sensor calculation constants */
#define OMAP_GRADIENT_SLOPE_W_PCB_4460  1142
#define OMAP_GRADIENT_CONST_W_PCB_4460  -393
#define OMAP_GRADIENT_SLOPE_W_PCB_4470  1063
#define OMAP_GRADIENT_CONST_W_PCB_4470  -477
#define AVERAGE_NUMBER	      20

struct omap_die_governor {
	struct thermal_dev *temp_sensor;
	void (*update_temp_thresh) (struct thermal_dev *, int min, int max);
	int report_rate;
	int panic_zone_reached;
	int decrease_mpu_freq_period;
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
	struct delayed_work decrease_mpu_freq_work;
	int gradient_slope;
	int gradient_const;
	int gradient_slope_w_pcb;
	int gradient_const_w_pcb;
	int prev_zone;
};

#define OMAP_THERMAL_ZONE_NAME_SZ	10
struct omap_thermal_zone {
	const char name[OMAP_THERMAL_ZONE_NAME_SZ];
	unsigned int cooling_increment;
	int temp_lower;
	int temp_upper;
	int update_rate;
	int average_rate;
};
#define OMAP_THERMAL_ZONE(n, i, l, u, r, a)		\
{							\
	.name				= n,		\
	.cooling_increment		= (i),		\
	.temp_lower			= (l),		\
	.temp_upper			= (u),		\
	.update_rate			= (r),		\
	.average_rate			= (a),		\
}

static struct omap_thermal_zone omap_thermal_zones[] = {
	OMAP_THERMAL_ZONE("safe", 0, OMAP_SAFE_TEMP, OMAP_MONITOR_TEMP,
			FAST_TEMP_MONITORING_RATE, NORMAL_TEMP_MONITORING_RATE),
	OMAP_THERMAL_ZONE("monitor", 0,
			OMAP_MONITOR_TEMP - HYSTERESIS_VALUE, OMAP_ALERT_TEMP,
			FAST_TEMP_MONITORING_RATE, FAST_TEMP_MONITORING_RATE),
	OMAP_THERMAL_ZONE("alert", 0,
			OMAP_ALERT_TEMP - HYSTERESIS_VALUE, OMAP_PANIC_TEMP,
			FAST_TEMP_MONITORING_RATE, FAST_TEMP_MONITORING_RATE),
	OMAP_THERMAL_ZONE("panic", 1,
			OMAP_PANIC_TEMP - HYSTERESIS_VALUE, OMAP_FATAL_TEMP,
			FAST_TEMP_MONITORING_RATE, FAST_TEMP_MONITORING_RATE),
};
static struct thermal_dev *therm_fw;
static struct omap_die_governor *omap_gov;
static int cpu_sensor_temp_table[AVERAGE_NUMBER];

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
		omap_gov->report_rate = thermal_device_call(temp_sensor,
						set_temp_report_rate, new_rate);
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
	int pcb_temp;

	pcb_temp = thermal_lookup_temp("pcb");
	if (pcb_temp >= 0 && (omap_gov->avg_is_valid == 1)) {
		omap_gov->pcb_temp = pcb_temp;
		if (omap_gov->pcb_temp < 0)
			return sensor_temp + omap_gov->absolute_delta;

		absolute_delta = (
			((omap_gov->avg_cpu_sensor_temp - omap_gov->pcb_temp) *
			omap_gov->gradient_slope_w_pcb / 1000) +
			omap_gov->gradient_const_w_pcb);

		/* Ensure that this formula never returns negative value */
		if (absolute_delta < 0)
			absolute_delta = 0;
	} else {
		absolute_delta = (
			(sensor_temp * omap_gov->gradient_slope / 1000) +
			omap_gov->gradient_const);
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
	if (thermal_lookup_temp("pcb") >= 0 && (omap_gov->avg_is_valid == 1))
		return hot_spot_temp - omap_gov->absolute_delta;
	else
		return ((hot_spot_temp - omap_gov->gradient_const) * 1000) /
			(1000 + omap_gov->gradient_slope);
}

static int omap_enter_zone(struct omap_thermal_zone *zone,
				bool set_cooling_level,
				struct list_head *cooling_list, int cpu_temp)
{
	int temp_upper;
	int temp_lower;

	if (list_empty(cooling_list)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	}

	if (set_cooling_level) {
		if (zone->cooling_increment)
			omap_gov->cooling_level += zone->cooling_increment;
		else
			omap_gov->cooling_level = 0;
		thermal_device_call_all(cooling_list, cool_device,
						omap_gov->cooling_level);
	}
	omap_gov->hotspot_temp_lower = zone->temp_lower;
	omap_gov->hotspot_temp_upper = zone->temp_upper;
	temp_lower = hotspot_temp_to_sensor_temp(omap_gov->hotspot_temp_lower);
	temp_upper = hotspot_temp_to_sensor_temp(omap_gov->hotspot_temp_upper);
	thermal_device_call(omap_gov->temp_sensor, set_temp_thresh, temp_lower,
								temp_upper);
	omap_update_report_rate(omap_gov->temp_sensor, zone->update_rate);
	if (thermal_lookup_temp("pcb") >= 0)
		omap_gov->average_period = zone->average_rate;

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

static void start_panic_guard(void)
{
	schedule_delayed_work(&omap_gov->decrease_mpu_freq_work,
		msecs_to_jiffies(omap_gov->decrease_mpu_freq_period));
}

static int omap_cpu_thermal_manager(struct list_head *cooling_list, int temp)
{
	int cpu_temp, zone = NO_ACTION;
	bool set_cooling_level = true;

	omap_gov->sensor_temp = temp;
	cpu_temp = convert_omap_sensor_temp_to_hotspot_temp(temp);

	if (cpu_temp >= OMAP_FATAL_TEMP) {
		omap_fatal_zone(cpu_temp);
		return FATAL_ZONE;
	} else if (cpu_temp >= OMAP_PANIC_TEMP) {
		int temp_upper;

		omap_gov->panic_zone_reached++;
		temp_upper = (((OMAP_FATAL_TEMP - OMAP_PANIC_TEMP) / 4) *
				omap_gov->panic_zone_reached) + OMAP_PANIC_TEMP;
		if (temp_upper >= OMAP_FATAL_TEMP)
			temp_upper = OMAP_FATAL_TEMP;
		omap_thermal_zones[PANIC_ZONE - 1].temp_upper = temp_upper;
		zone = PANIC_ZONE;
	} else if (cpu_temp < (OMAP_PANIC_TEMP - HYSTERESIS_VALUE)) {
		if (cpu_temp >= OMAP_ALERT_TEMP) {
			set_cooling_level = omap_gov->panic_zone_reached == 0;
			zone = ALERT_ZONE;
		} else if (cpu_temp < (OMAP_ALERT_TEMP - HYSTERESIS_VALUE)) {
			if (cpu_temp >= OMAP_MONITOR_TEMP) {
				omap_gov->panic_zone_reached = 0;
				zone = MONITOR_ZONE;
			} else {
				/*
				 * this includes the case where :
				 * (OMAP_MONITOR_TEMP - HYSTERESIS_VALUE) <= T
				 * && T < OMAP_MONITOR_TEMP
				 */
				omap_gov->panic_zone_reached = 0;
				zone = SAFE_ZONE;
			}
		} else {
			/*
			 * this includes the case where :
			 * (OMAP_ALERT_TEMP - HYSTERESIS_VALUE) <= T
			 * && T < OMAP_ALERT_TEMP
			 */
			omap_gov->panic_zone_reached = 0;
			zone = MONITOR_ZONE;
		}
	} else {
		/*
		 * this includes the case where :
		 * (OMAP_PANIC_TEMP - HYSTERESIS_VALUE) <= T < OMAP_PANIC_TEMP
		 */
		set_cooling_level = omap_gov->panic_zone_reached == 0;
		zone = ALERT_ZONE;
	}

	if (zone != NO_ACTION) {
		struct omap_thermal_zone *therm_zone;

		therm_zone = &omap_thermal_zones[zone - 1];
		if (omap_gov->panic_zone_reached)
			start_panic_guard();
		else
			cancel_delayed_work(&omap_gov->decrease_mpu_freq_work);

		if ((omap_gov->prev_zone != zone) || (zone == PANIC_ZONE)) {
			pr_info("%s:sensor %d avg sensor %d pcb ",
				 __func__, temp,
				 omap_gov->avg_cpu_sensor_temp);
			pr_info("%d, delta %d hot spot %d\n",
				 omap_gov->pcb_temp, omap_gov->absolute_delta,
				 cpu_temp);
			pr_info("%s: hot spot temp %d - going into %s zone\n",
				__func__, cpu_temp, therm_zone->name);
			omap_gov->prev_zone = zone;
		}
		omap_enter_zone(therm_zone, set_cooling_level,
				cooling_list, cpu_temp);
	}

	return zone;
}

static void decrease_mpu_freq_fn(struct work_struct *work)
{
	struct omap_die_governor *omap_gov;

	omap_gov = container_of(work, struct omap_die_governor,
				decrease_mpu_freq_work.work);

	omap_gov->sensor_temp = thermal_request_temp(omap_gov->temp_sensor);
	thermal_sensor_set_temp(omap_gov->temp_sensor);
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
	thermal_device_call(omap_gov->temp_sensor, set_temp_thresh,
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

static int omap_process_cpu_temp(struct thermal_dev *gov,
				struct list_head *cooling_list,
				struct thermal_dev *temp_sensor,
				int temp)
{
	if (!omap_gov->temp_sensor)
		omap_gov->temp_sensor = temp_sensor;
	return omap_cpu_thermal_manager(cooling_list, temp);
}

static int omap_die_pm_notifier_cb(struct notifier_block *notifier,
				unsigned long pm_event,  void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		cancel_delayed_work_sync(&omap_gov->average_cpu_sensor_work);
		cancel_delayed_work_sync(&omap_gov->decrease_mpu_freq_work);
		break;
	case PM_POST_SUSPEND:
		schedule_work(&omap_gov->average_cpu_sensor_work.work);
		break;
	}

	return NOTIFY_DONE;
}

static struct thermal_dev_ops omap_gov_ops = {
	.process_temp = omap_process_cpu_temp,
};

static struct notifier_block omap_die_pm_notifier = {
	.notifier_call = omap_die_pm_notifier_cb,
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

	if (cpu_is_omap446x()) {
		omap_gov->gradient_slope = OMAP_GRADIENT_SLOPE_4460;
		omap_gov->gradient_const = OMAP_GRADIENT_CONST_4460;
		omap_gov->gradient_slope_w_pcb = OMAP_GRADIENT_SLOPE_W_PCB_4460;
		omap_gov->gradient_const_w_pcb = OMAP_GRADIENT_CONST_W_PCB_4460;
	} else if (cpu_is_omap447x()) {
		omap_gov->gradient_slope = OMAP_GRADIENT_SLOPE_4470;
		omap_gov->gradient_const = OMAP_GRADIENT_CONST_4470;
		omap_gov->gradient_slope_w_pcb = OMAP_GRADIENT_SLOPE_W_PCB_4470;
		omap_gov->gradient_const_w_pcb = OMAP_GRADIENT_CONST_W_PCB_4470;
	} else {
		omap_gov->gradient_slope = 0;
		omap_gov->gradient_const = 0;
		omap_gov->gradient_slope_w_pcb = 0;
		omap_gov->gradient_const_w_pcb = 0;
	}

	/* Init delayed work to average on-die temperature */
	INIT_DELAYED_WORK(&omap_gov->average_cpu_sensor_work,
			  average_cpu_sensor_delayed_work_fn);
	INIT_DELAYED_WORK(&omap_gov->decrease_mpu_freq_work,
			  decrease_mpu_freq_fn);

	omap_gov->average_period = NORMAL_TEMP_MONITORING_RATE;
	omap_gov->decrease_mpu_freq_period = DECREASE_MPU_FREQ_PERIOD;
	omap_gov->avg_is_valid = 0;

	if (register_pm_notifier(&omap_die_pm_notifier))
		pr_err("%s: omap_die pm registration failed!\n", __func__);

	schedule_delayed_work(&omap_gov->average_cpu_sensor_work,
			msecs_to_jiffies(0));

	return 0;
}

static void __exit omap_die_governor_exit(void)
{
	cancel_delayed_work_sync(&omap_gov->average_cpu_sensor_work);
	cancel_delayed_work_sync(&omap_gov->decrease_mpu_freq_work);
	thermal_governor_dev_unregister(therm_fw);
	kfree(therm_fw);
	kfree(omap_gov);
}

module_init(omap_die_governor_init);
module_exit(omap_die_governor_exit);

MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
MODULE_DESCRIPTION("OMAP on-die thermal governor");
MODULE_LICENSE("GPL");
