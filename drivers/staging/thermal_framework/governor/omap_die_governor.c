/*
 * drivers/staging/thermal_framework/governor/omap_die_governor.c
 *
 * Copyright (C) 2011-2012 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Dan Murphy <DMurphy@ti.com>
 *
 * Contributors:
 *	Sebastien Sabatier <s-sabatier1@ti.com>
 *	Margarita Olaya Cabrera <magi.olaya@ti.com>
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
#include <linux/debugfs.h>

#include <linux/omap_die_governor.h>
#include <linux/thermal_framework.h>

#include <plat/cpu.h>
#include <linux/opp.h>
#include <plat/omap_device.h>

/* Zone information */
#define SHUTDOWN_ZONE	5
#define CRITICAL_ZONE	4
#define ALERT_ZONE	3
#define MONITOR_ZONE	2
#define SAFE_ZONE	1
#define NO_ACTION	0
#define MAX_NO_MON_ZONES CRITICAL_ZONE
#define OMAP_SHUTDOWN_TEMP			125000
#define OMAP_SHUTDOWN_TEMP_ES1_0		120000
#define OMAP_CRITICAL_TEMP			110000
#define OMAP_CRITICAL_TEMP_ES1_0		105000
#define OMAP_ALERT_TEMP			100000
#define OMAP_MONITOR_TEMP		85000
#define OMAP_SAFE_TEMP			25000

#define OMAP_SAFE_ZONE_MAX_TREND	400
#define OMAP_MONITOR_ZONE_MAX_TREND	250
#define OMAP_ALERT_ZONE_MAX_TREND	150
#define OMAP_PANIC_ZONE_MAX_TREND	50

/* TODO: Define this via a configurable file */
#define HYSTERESIS_VALUE		5000
#define NORMAL_TEMP_MONITORING_RATE	1000
#define FAST_TEMP_MONITORING_RATE	250
#define FAST_TEMP_MONITORING_RATE_ES1_0	125
#define AVERAGE_NUMBER			20
#define SAME_ZONE_CNT			600

enum governor_instances {
	OMAP_GOV_CPU_INSTANCE,
	OMAP_GOV_GPU_INSTANCE,
	OMAP_GOV_MAX_INSTANCE,
};

struct omap_governor {
	struct thermal_dev *temp_sensor;
	struct thermal_dev thermal_fw;
	struct omap_thermal_zone zones[MAX_NO_MON_ZONES];
	void (*update_temp_thresh) (struct thermal_dev *, int min, int max);
	int report_rate;
	int critical_zone_reached;
	int cooling_level;
	int hotspot_temp_upper;
	int hotspot_temp_lower;
	int hotspot_temp;
	int pcb_temp;
	bool pcb_sensor_available;
	bool bursting;
	int sensor_temp;
	int absolute_delta;
	int omap_gradient_slope;
	int omap_gradient_const;
	int prev_zone;
	int prev_cool_level;
	int trend;
	int steps;
	int same_zone_cnt;
	int is_stable;
	bool enable_debug_print;
	/* for synchronizing actions */
	struct mutex mutex;
};

/* Initial set of thersholds for different thermal zones */
static struct omap_thermal_zone zones_es1_0[] __initdata = {
	OMAP_THERMAL_ZONE("safe", 0, OMAP_SAFE_TEMP, OMAP_MONITOR_TEMP,
			FAST_TEMP_MONITORING_RATE_ES1_0,
			NORMAL_TEMP_MONITORING_RATE, OMAP_SAFE_ZONE_MAX_TREND),
	OMAP_THERMAL_ZONE("monitor", 0,
			OMAP_MONITOR_TEMP - HYSTERESIS_VALUE, OMAP_ALERT_TEMP,
			FAST_TEMP_MONITORING_RATE_ES1_0,
			FAST_TEMP_MONITORING_RATE_ES1_0,
			OMAP_MONITOR_ZONE_MAX_TREND),
	OMAP_THERMAL_ZONE("alert", 0,
			OMAP_ALERT_TEMP - HYSTERESIS_VALUE,
			OMAP_CRITICAL_TEMP_ES1_0,
			FAST_TEMP_MONITORING_RATE_ES1_0,
			FAST_TEMP_MONITORING_RATE_ES1_0,
			OMAP_ALERT_ZONE_MAX_TREND),
	OMAP_THERMAL_ZONE("critical", 1,
			OMAP_CRITICAL_TEMP_ES1_0 - HYSTERESIS_VALUE,
			OMAP_SHUTDOWN_TEMP_ES1_0,
			FAST_TEMP_MONITORING_RATE_ES1_0,
			FAST_TEMP_MONITORING_RATE_ES1_0,
			OMAP_PANIC_ZONE_MAX_TREND),
};

/* Initial set of thersholds for different thermal zones starting from ES2.0 */
static struct omap_thermal_zone zones_es2_0[] __initdata = {
	OMAP_THERMAL_ZONE("safe", 0, OMAP_SAFE_TEMP, OMAP_MONITOR_TEMP,
			FAST_TEMP_MONITORING_RATE,
			NORMAL_TEMP_MONITORING_RATE, OMAP_SAFE_ZONE_MAX_TREND),
	OMAP_THERMAL_ZONE("monitor", 0,
			OMAP_MONITOR_TEMP - HYSTERESIS_VALUE, OMAP_ALERT_TEMP,
			FAST_TEMP_MONITORING_RATE,
			FAST_TEMP_MONITORING_RATE,
			OMAP_MONITOR_ZONE_MAX_TREND),
	OMAP_THERMAL_ZONE("alert", 0,
			OMAP_ALERT_TEMP - HYSTERESIS_VALUE,
			OMAP_CRITICAL_TEMP,
			FAST_TEMP_MONITORING_RATE,
			FAST_TEMP_MONITORING_RATE,
			OMAP_ALERT_ZONE_MAX_TREND),
	OMAP_THERMAL_ZONE("critical", 1,
			OMAP_CRITICAL_TEMP - HYSTERESIS_VALUE,
			OMAP_SHUTDOWN_TEMP,
			FAST_TEMP_MONITORING_RATE,
			FAST_TEMP_MONITORING_RATE,
			OMAP_PANIC_ZONE_MAX_TREND),
};

static struct omap_die_governor_pdata *omap_gov_pdata;

static struct omap_governor *omap_gov_instance[OMAP_GOV_MAX_INSTANCE];

/**
 * DOC: Introduction
 * =================
 * The OMAP On-Die Temperature governor maintains the policy for the
 * on die temperature sensor.  The main goal of the governor is to receive
 * a temperature report from a thermal sensor and calculate the current
 * thermal zone.  Each zone will sort through a list of cooling agents
 * passed in to determine the correct cooling strategy that will cool the
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
 * SHUTDOWN_ZONE: This zone indicates that the on-die temperature has reached
 * a point where the device needs to be rebooted and allow ROM or the bootloader
 * to run to allow the device to cool.
 *
 * CRITICAL_ZONE: This zone indicates a near shutdown temperature is approaching
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
static void omap_update_report_rate(struct omap_governor *omap_gov,
					struct thermal_dev *temp_sensor,
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
 *
 * @sensor_temp: Raw temperature reported by the OMAP die temp sensor
 *
 * Returns the calculated hot spot temperature for the zone calculation
 */
static signed int convert_omap_sensor_temp_to_hotspot_temp(
				struct omap_governor *omap_gov, int sensor_temp)
{
	int absolute_delta;

	int avg_temp = 0;

	/* PCB sensor inputs are required only for CPU domain */
	if ((!strcmp(omap_gov->thermal_fw.domain_name, "cpu")) &&
		omap_gov->pcb_sensor_available &&
		omap_gov->temp_sensor &&
		((avg_temp = thermal_lookup_avg("cpu")) > 0)) {
		omap_gov->pcb_temp  = thermal_lookup_temp("pcb");
		if (omap_gov->pcb_temp < 0)
			return sensor_temp + omap_gov->absolute_delta;

		absolute_delta = (
			((avg_temp - omap_gov->pcb_temp) *
				thermal_lookup_slope("pcb",
				omap_gov->thermal_fw.domain_name) / 1000) +
				thermal_lookup_offset("pcb",
				omap_gov->thermal_fw.domain_name));
		/* Ensure that this formula never returns negative value */
		if (absolute_delta < 0)
			absolute_delta = 0;
	} else {
		absolute_delta = ((sensor_temp *
					omap_gov->omap_gradient_slope / 1000) +
					omap_gov->omap_gradient_const);
	}

	omap_gov->absolute_delta = absolute_delta;

	pr_debug("%s: sensor.temp -> hotspot.temp: sensor %d avg_sensor %d pcb %d, delta %d hotspot %d\n",
			omap_gov->thermal_fw.domain_name,
			sensor_temp, avg_temp,
			omap_gov->pcb_temp, omap_gov->absolute_delta,
			sensor_temp + absolute_delta);

	omap_gov->hotspot_temp = sensor_temp + absolute_delta;
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
 * Returns the calculated on-die temperature.
 */

static signed hotspot_temp_to_sensor_temp(struct omap_governor *omap_gov,
							int hot_spot_temp)
{
	/* PCB sensor inputs are required only for CPU domain */
	if ((!strcmp(omap_gov->thermal_fw.domain_name, "cpu")) &&
		omap_gov->pcb_sensor_available && omap_gov->temp_sensor
		&& (thermal_lookup_avg("cpu") > 0))
		return hot_spot_temp - omap_gov->absolute_delta;
	else
		return ((hot_spot_temp - omap_gov->omap_gradient_const) *
				1000) / (1000 + omap_gov->omap_gradient_slope);
}

static int omap_enter_zone(struct omap_governor *omap_gov,
				int current_zone,
				bool in_new_zone,
				bool set_cooling_level,
				struct list_head *cooling_list, int cpu_temp,
				int inter_zone_thot)
{
	int temp_upper;
	int temp_lower;
	int temp_cool_level;

	struct omap_thermal_zone *zone = &omap_gov->zones[current_zone-1];
	omap_gov->trend =
		thermal_lookup_trend(omap_gov->temp_sensor->domain_name);
	omap_gov->bursting = omap_gov->trend > zone->max_trend;
	omap_gov->is_stable = false;
	omap_gov->prev_cool_level = temp_cool_level = omap_gov->cooling_level;

	if (in_new_zone)
		omap_gov->same_zone_cnt = 0;
	else
		omap_gov->same_zone_cnt++;

	if (omap_gov->same_zone_cnt >= SAME_ZONE_CNT &&
		(thermal_check_temp_stability(omap_gov->temp_sensor) > 0))
		omap_gov->is_stable = true;

	if (!in_new_zone && !omap_gov->bursting
			&& !omap_gov->is_stable)
		return 0;

	if (omap_gov->is_stable) {
		if ((omap_gov->cooling_level > 0) &&
		    (current_zone != CRITICAL_ZONE)) {
			pr_debug("%s temperature stabilized unthrottling\n",
				 __func__);
			temp_cool_level--;
			/*
			 * As there is a dependency between cooling level and
			 * critical subzone (e.g.
			 * 1st subzone = 1st cooling level,
			 * 2nd subzone = 2nd cooling level, ....), we need to
			 * decrement critical_zone_reached when decrementing
			 * cooling level, so that this dependency won't be
			 * broken.
			 */
			if (omap_gov->critical_zone_reached)
				omap_gov->critical_zone_reached--;
		}
	}

	if (list_empty(cooling_list)) {
		pr_err("%s: No Cooling devices registered\n",
			__func__);
		return -ENODEV;
	}

	if (omap_gov->bursting) {
		pr_debug("%s(%s): throttling due to fast temp rise:\n",
			__func__, omap_gov->thermal_fw.domain_name);
		pr_debug("%d mC, %d mC, %d mC/ms, %d mC/ms\n",
			cpu_temp, omap_gov->hotspot_temp, omap_gov->trend,
			zone->max_trend);
			temp_cool_level++;
	}

	if (set_cooling_level && !omap_gov->bursting) {
		if (zone->cooling_increment)
			temp_cool_level += zone->cooling_increment;
		else
			temp_cool_level = 0;
	}

	if (temp_cool_level < 0)
		omap_gov->cooling_level = 0;
	else if (temp_cool_level > omap_gov->steps)
		omap_gov->cooling_level = omap_gov->steps;
	else
		omap_gov->cooling_level = temp_cool_level;

	if (omap_gov->prev_cool_level != omap_gov->cooling_level)
		thermal_device_call_all(cooling_list, cool_device,
				omap_gov->cooling_level);

	if (inter_zone_thot == 0)
		inter_zone_thot = zone->temp_upper;

	temp_lower = hotspot_temp_to_sensor_temp(omap_gov, zone->temp_lower);
	temp_upper = hotspot_temp_to_sensor_temp(omap_gov, inter_zone_thot);
	thermal_device_call(omap_gov->temp_sensor, set_temp_thresh, temp_lower,
								temp_upper);
	omap_update_report_rate(omap_gov, omap_gov->temp_sensor,
							zone->update_rate);

	omap_gov->hotspot_temp_lower = temp_lower;
	omap_gov->hotspot_temp_upper = temp_upper;

	/* PCB sensor inputs are required only for CPU domain */
	if ((!strcmp(omap_gov->thermal_fw.domain_name, "cpu")) &&
		omap_gov->pcb_sensor_available)
		thermal_set_avg_period(omap_gov->temp_sensor,
						zone->average_rate);

	return 0;
}

/**
 * omap_shutdown_zone() - Shut-down the system to ensure OMAP Junction
 *			temperature decreases enough
 *
 * @cpu_temp:	The current adjusted CPU temperature
 *
 * No return forces a restart of the system
 */
static void omap_shutdown_zone(int cpu_temp)
{
	pr_emerg("%s:SHUTDOWN ZONE (hot spot temp: %i)\n", __func__, cpu_temp);

	kernel_restart(NULL);
}

static int omap_thermal_match_zone(struct omap_thermal_zone *zone_set, int temp)
{
	int i;

	for (i = MAX_NO_MON_ZONES - 1; i >= 0; i--)
		if (zone_set[i].temp_upper < temp)
			return i + 2; /* no action and index 0*/

	return temp > OMAP_SAFE_TEMP ? SAFE_ZONE : NO_ACTION;
}

static int omap_thermal_manager(struct omap_governor *omap_gov,
				struct list_head *cooling_list, int temp)
{
	int cpu_temp, zone = NO_ACTION;
	bool set_cooling_level = true;
	int temp_upper, inter_zone_thot = 0;
	int shutdown, alert;
	bool in_new_zone = false;

	int avg_temp = thermal_lookup_avg(omap_gov->temp_sensor->domain_name);

	cpu_temp = convert_omap_sensor_temp_to_hotspot_temp(omap_gov, temp);
	zone = omap_thermal_match_zone(omap_gov->zones, cpu_temp);

	switch (zone) {
	case SHUTDOWN_ZONE:
		omap_shutdown_zone(cpu_temp);
		return SHUTDOWN_ZONE;
	case CRITICAL_ZONE:
		alert = omap_gov->zones[ALERT_ZONE - 1].temp_upper;
		shutdown = omap_gov->zones[CRITICAL_ZONE - 1].temp_upper;
		temp_upper = (((shutdown - alert) / omap_gov->steps) *
				omap_gov->critical_zone_reached) + alert;
		if (temp_upper < cpu_temp) {
			omap_gov->critical_zone_reached++;
			in_new_zone = true;
		} else {
			set_cooling_level = false; /* no need for update */
			in_new_zone = false;
		}
		temp_upper = (((shutdown - alert) / omap_gov->steps) *
				omap_gov->critical_zone_reached) + alert;
		if (temp_upper >= shutdown)
			temp_upper = shutdown;

		inter_zone_thot = temp_upper;

		break;
	case ALERT_ZONE:
		set_cooling_level = omap_gov->critical_zone_reached == 0;
		break;
	case MONITOR_ZONE:
	case SAFE_ZONE:
		omap_gov->critical_zone_reached = 0;
		break;
	case NO_ACTION:
		break;
	default:
		break;
	};

	if (zone != NO_ACTION) {
		struct omap_thermal_zone *therm_zone;

		if (zone != omap_gov->prev_zone)
			in_new_zone = true;


		therm_zone = &omap_gov->zones[zone - 1];
		pr_debug("%s: %s in new zone %d %s: %d (%d %d)\n",
			__func__, omap_gov->thermal_fw.domain_name,
			in_new_zone, therm_zone->name, cpu_temp, zone,
			omap_gov->prev_zone);
		if ((omap_gov->enable_debug_print) &&
		((omap_gov->prev_zone != zone) || (zone == CRITICAL_ZONE))) {
			pr_info("%s:sensor %d avg sensor %d pcb ",
				__func__, temp, avg_temp);
			pr_info("%d, delta %d hot spot %d\n",
				 omap_gov->pcb_temp, omap_gov->absolute_delta,
				 cpu_temp);
			pr_info("%s: hot spot temp %d - going into %s zone\n",
				__func__, cpu_temp, therm_zone->name);
		}
		omap_gov->prev_zone = zone;

		omap_enter_zone(omap_gov, zone, in_new_zone, set_cooling_level,
				cooling_list, cpu_temp, inter_zone_thot);
	}

	return zone;
}

static int omap_process_temp(struct thermal_dev *gov,
				struct list_head *cooling_list,
				struct thermal_dev *temp_sensor,
				int temp)
{
	struct omap_governor *omap_gov = container_of(gov, struct
					omap_governor, thermal_fw);
	int ret;

	mutex_lock(&omap_gov->mutex);
	omap_gov->temp_sensor = temp_sensor;
	if (!omap_gov->pcb_sensor_available &&
		(thermal_check_domain("pcb") == 0))
		omap_gov->pcb_sensor_available = true;

	/* because here we are safe, we do an extra read */
	temp = thermal_request_temp(omap_gov->temp_sensor);
	omap_gov->sensor_temp = temp;

	pr_debug("%s: received temp %i on %s\n", __func__, temp,
				omap_gov->thermal_fw.domain_name);

	ret = omap_thermal_manager(omap_gov, cooling_list, temp);
	mutex_unlock(&omap_gov->mutex);

	return ret;
}

/* debugfs hooks for omap die gov */
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

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(omap_die_gov_fops, option_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(omap_die_gov_rw_fops, option_get, option_set, "%llu\n");

/* Update temp_sensor with current values */
static void debug_apply_thresholds(struct omap_governor *omap_gov)
{
	/*
	 * Reconfigure the current temperature thresholds according
	 * to the new user space thresholds.
	 */
	thermal_device_call(omap_gov->temp_sensor, set_temp_thresh,
				omap_gov->hotspot_temp_lower,
				omap_gov->hotspot_temp_upper);
	omap_gov->sensor_temp = thermal_device_call(omap_gov->temp_sensor,
						    report_temp);
	thermal_sensor_set_temp(omap_gov->temp_sensor);
}

static int option_alert_get(void *data, u64 *val)
{
	struct omap_governor *omap_gov = (struct omap_governor *) data;

	*val = omap_gov->zones[MONITOR_ZONE - 1].temp_upper;

	return 0;
}

static int option_alert_set(void *data, u64 val)
{
	struct omap_governor *omap_gov = (struct omap_governor *) data;
	int thres = omap_gov->zones[ALERT_ZONE - 1].temp_upper;

	if (val <= OMAP_MONITOR_TEMP) {
		pr_err("Invalid threshold: ALERT:%d is <= MONITOR:%d\n",
			(int)val, OMAP_MONITOR_TEMP);
		return -EINVAL;
	} else if (val >= thres) {
		pr_err("Invalid threshold: ALERT:%d is >= CRITICAL:%d\n",
			(int)val, thres);
		return -EINVAL;
	}
	/* Also update the governor zone array */
	omap_gov->zones[MONITOR_ZONE - 1].temp_upper = val;
	omap_gov->zones[ALERT_ZONE - 1].temp_lower = val - HYSTERESIS_VALUE;

	/* Skip sensor update if no sensor is present */
	if (!IS_ERR_OR_NULL(omap_gov->temp_sensor))
		debug_apply_thresholds(omap_gov);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(omap_die_gov_alert_fops, option_alert_get,
			option_alert_set, "%llu\n");

static int option_critical_get(void *data, u64 *val)
{
	struct omap_governor *omap_gov = (struct omap_governor *) data;

	*val = omap_gov->zones[ALERT_ZONE - 1].temp_upper;

	return 0;
}

static int option_critical_set(void *data, u64 val)
{
	struct omap_governor *omap_gov = (struct omap_governor *) data;
	int thres = omap_gov->zones[MONITOR_ZONE - 1].temp_upper;

	if (val <= thres) {
		pr_err("Invalid threshold: CRITICAL:%d is <= ALERT:%d\n",
			(int)val, thres);
		return -EINVAL;
	} else if (val >= OMAP_SHUTDOWN_TEMP) {
		pr_err("Invalid threshold: CRITICAL:%d is >= SHUTDOWN:%d\n",
			(int)val, OMAP_SHUTDOWN_TEMP);
		return -EINVAL;
	}
	/* Also update the governor zone array */
	omap_gov->zones[ALERT_ZONE - 1].temp_upper = val;
	omap_gov->zones[CRITICAL_ZONE - 1].temp_lower = val - HYSTERESIS_VALUE;

	/* Skip sensor update if no sensor is present */
	if (!IS_ERR_OR_NULL(omap_gov->temp_sensor))
		debug_apply_thresholds(omap_gov);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(omap_die_gov_critical_fops, option_critical_get,
			option_critical_set, "%llu\n");

#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
static int omap_gov_register_debug_entries(struct thermal_dev *gov,
					struct dentry *d)
{
	struct omap_governor *omap_gov = container_of(gov, struct
						omap_governor, thermal_fw);

	/* Read Only - Debug properties of cpu and gpu gov */
	(void) debugfs_create_file("cooling_level",
			S_IRUGO, d, &(omap_gov->cooling_level),
			&omap_die_gov_fops);
	(void) debugfs_create_file("hotspot_temp_upper",
			S_IRUGO, d, &(omap_gov->hotspot_temp_upper),
			&omap_die_gov_fops);
	(void) debugfs_create_file("hotspot_temp_lower",
			S_IRUGO, d, &(omap_gov->hotspot_temp_lower),
			&omap_die_gov_fops);
	(void) debugfs_create_file("hotspot_temp",
			S_IRUGO, d, &(omap_gov->hotspot_temp),
			&omap_die_gov_fops);
	(void) debugfs_create_file("critical_sub_zones",
			S_IRUGO, d, &(omap_gov->steps),
			&omap_die_gov_fops);


	/* Read  and Write properties of die gov */
	/* ALERT zone threshold */
	(void) debugfs_create_file("alert_threshold",
			S_IRUGO | S_IWUSR, d, omap_gov,
			&omap_die_gov_alert_fops);

	/* CRITICAL zone threshold */
	(void) debugfs_create_file("critical_threshold",
			S_IRUGO | S_IWUSR, d, omap_gov,
			&omap_die_gov_critical_fops);

	/* Flag to enable the Debug Zone Prints */
	(void) debugfs_create_file("enable_debug_print",
			S_IRUGO | S_IWUSR, d, &(omap_gov->enable_debug_print),
			&omap_die_gov_rw_fops);

	return 0;
}
#endif

static struct thermal_dev_ops omap_gov_ops = {
	.process_temp = omap_process_temp,
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	.register_debug_entries = omap_gov_register_debug_entries,
#endif
};

void omap_die_governor_register_pdata(struct omap_die_governor_pdata *omap_gov)
{
	if (omap_gov)
		omap_gov_pdata = omap_gov;
}
EXPORT_SYMBOL_GPL(omap_die_governor_register_pdata);

static int __init omap_governor_init(void)
{
	int i;
	bool pdata_valid = false;
	struct device *mpu, *gpu;

	for (i = 0; i < OMAP_GOV_MAX_INSTANCE; i++) {
		omap_gov_instance[i] = kzalloc(sizeof(struct omap_governor),
								GFP_KERNEL);
		if (!omap_gov_instance[i]) {
			pr_err("%s:Cannot allocate memory\n", __func__);
			goto error;
		}
	}

	for (i = 0; i < OMAP_GOV_MAX_INSTANCE; i++) {
		mutex_init(&omap_gov_instance[i]->mutex);
		omap_gov_instance[i]->hotspot_temp = 0;
		omap_gov_instance[i]->critical_zone_reached = 0;
		omap_gov_instance[i]->pcb_sensor_available = false;
		omap_gov_instance[i]->enable_debug_print = false;
	}

	if (omap_gov_pdata && omap_gov_pdata->zones) {
		if (omap_gov_pdata->zones_num != MAX_NO_MON_ZONES)
			WARN(1, "%s: pdata is invalid\n", __func__);
		else
			pdata_valid = true;
	}

	/* Initializing CPU governor */
	if (pdata_valid) {
		memcpy(omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->zones,
		       omap_gov_pdata->zones, omap_gov_pdata->zones_num *
		       sizeof(struct omap_thermal_zone));
	} else if (omap_rev() == OMAP5430_REV_ES1_0 ||
	    omap_rev() == OMAP5432_REV_ES1_0) {
		memcpy(omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->zones,
		       zones_es1_0, sizeof(zones_es1_0));
	} else {
		memcpy(omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->zones,
		       zones_es2_0, sizeof(zones_es2_0));
	}

	mpu = omap_device_get_by_hwmod_name("mpu");
	if (!mpu) {
		pr_err("%s: mpu domain does not know the amount of throttling",
			__func__);
		goto error;
	}
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->steps =
						opp_get_opp_count(mpu) - 1;
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->thermal_fw.name =
							"omap_cpu_governor";
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->thermal_fw.domain_name =
							"cpu";
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->thermal_fw.dev_ops =
							&omap_gov_ops;
	thermal_governor_dev_register(
			&omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->thermal_fw);

	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->omap_gradient_slope =
	thermal_get_slope(&omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->thermal_fw,
									NULL);
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->omap_gradient_const =
	thermal_get_offset(&omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->
							thermal_fw, NULL);

	pr_info("%s: domain %s slope %d const %d\n", __func__,
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->thermal_fw.domain_name,
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->omap_gradient_slope,
	omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->omap_gradient_const);

	/* Initializing GPU governor */
	if (pdata_valid) {
		memcpy(omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->zones,
		       omap_gov_pdata->zones, omap_gov_pdata->zones_num *
		       sizeof(struct omap_thermal_zone));
	} else if (omap_rev() == OMAP5430_REV_ES1_0 ||
	    omap_rev() == OMAP5432_REV_ES1_0) {
		memcpy(omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->zones,
		       zones_es1_0, sizeof(zones_es1_0));
	} else {
		memcpy(omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->zones,
		       zones_es2_0, sizeof(zones_es2_0));
	}

	gpu = omap_device_get_by_hwmod_name("gpu");
	if (!gpu) {
		pr_err("%s: gpu domain does not know the amount of throttling",
			__func__);
		goto mpu_free;
	}
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->steps =
						opp_get_opp_count(gpu) - 1;
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->thermal_fw.name =
							"omap_gpu_governor";
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->thermal_fw.domain_name =
							"gpu";
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->thermal_fw.dev_ops =
							&omap_gov_ops;
	thermal_governor_dev_register(
			&omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->thermal_fw);

	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->omap_gradient_slope =
	thermal_get_slope(&omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->thermal_fw,
									NULL);
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->omap_gradient_const =
	thermal_get_offset(&omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->
							thermal_fw, NULL);

	pr_info("%s: domain %s slope %d const %d\n", __func__,
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->thermal_fw.domain_name,
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->omap_gradient_slope,
	omap_gov_instance[OMAP_GOV_GPU_INSTANCE]->omap_gradient_const);

	return 0;

mpu_free:
	thermal_governor_dev_unregister(
		&omap_gov_instance[OMAP_GOV_CPU_INSTANCE]->thermal_fw);
error:
	for (i = 0; i < OMAP_GOV_MAX_INSTANCE; i++)
		kfree(omap_gov_instance[i]);

	return -ENOMEM;
}

static void __exit omap_governor_exit(void)
{
	int i;

	for (i = 0; i < OMAP_GOV_MAX_INSTANCE; i++) {
		thermal_governor_dev_unregister(
					&omap_gov_instance[i]->thermal_fw);
		kfree(omap_gov_instance[i]);
	}
}

module_init(omap_governor_init);
module_exit(omap_governor_exit);

MODULE_AUTHOR("Dan Murphy <DMurphy@ti.com>");
MODULE_DESCRIPTION("OMAP on-die thermal governor");
MODULE_LICENSE("GPL");
