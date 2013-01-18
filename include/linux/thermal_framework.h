/*
 * Thermal Framework Driver
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

#ifndef __LINUX_THERMAL_FRAMEWORK_H__
#define __LINUX_THERMAL_FRAMEWORK_H__

#include <linux/seq_file.h>
#include <linux/dcache.h>

#define AVERAGE_NUMBER 20
#define MAX_AVG_PERIOD 5000
#define STABLE_TREND_COUNT 10

struct thermal_dev;
struct thermal_domain;

/**
 * struct thermal_dev_ops  - Structure for device operation call backs
 * @get_temp: A temp sensor call back to get the current temperature.
 *		temp is reported in milli degrees.
 * @set_temp_thresh: Update the temperature sensor thresholds.  This can be used
 *		to allow the sensor to only report changes when the thresholds
 *		have been crossed.
 * @set_temp_report_rate: Update the rate at which the temperature sensor
 *		reports the temperature change.  This API should return the
*		current measurement rate that the sensor is measuring at.
 * @cool_device: The cooling agent call back to process a list of cooling agents
 * @process_temp: The governors call back for processing a domain temperature
 *
 */
struct thermal_dev_ops {
	/* Sensor call backs */
	int (*report_temp) (struct thermal_dev *);
	int (*set_temp_thresh) (struct thermal_dev *temp_sensor,
			int min, int max);
	int (*set_temp_report_rate) (struct thermal_dev *, int rate);
	int (*init_slope) (struct thermal_dev *, const char *rel);
	int (*init_offset) (struct thermal_dev *, const char *rel);
	/* Cooling agent call backs */
	int (*cool_device) (struct thermal_dev *, int temp);
	/* Governor call backs */
	int (*process_temp) (struct thermal_dev *gov,
				struct list_head *cooling_list,
				struct thermal_dev *temp_sensor, int temp);
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	/* Debugging interface */
	int (*debug_report) (struct thermal_dev *, struct seq_file *s);
	int (*register_debug_entries) (struct thermal_dev *, struct dentry *d);
	/* copy of report_temp, while doing temperature injections */
	int (*orig_report) (struct thermal_dev *);
#endif
};

/**
 * struct thermal_cooling_action  - Structure for each action to reduce temp.
 * @priority: This action must be taken when there is a message with cooling
 *            level / priority equals to @priority
 * @reduction: The reduction from maximum value in percentage that needs
 *             to be taken when executing this action.
 */
struct thermal_cooling_action {
	unsigned int priority;
	unsigned int reduction;
	struct list_head node;
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	struct dentry *d;
#endif
};

/**
 * struct stats_thermal - Structure containing themal statistics of a domain.
 * @name: The name of the domain.
 * @avg: Average temperature of a domain.
 * @avg_period: Time gap between 2 consecutive readings
 * @sensor_temp_table: Table containing the latest AVERAGE_NUMBER readings.
 * @current_temp: Current temperature of the domain.
 * @acc_is_valid: Flag set to true only when AVERAGE_NUMBER number of readings
			are taken.
 * @avg_enabled: Flag whic tells if averaging feature is enabled or not.
 * @avg_num: Number of samples to be considered for average computation.
 * @trend: Trend of the particular domain.
 * @trending_enabled: Flag that shows if trending is enabled.
 * @safe_temp_trend: Trend threshold for a particular domain.
 * @stable_cnt: Count which tells the number of stable trends.
 * @is_stable: Flag which tells if the domain temperature is stable.
 * @accumulation_enabled: Flag which tells if accumulation is enabled.
 * @sample_index: The current array index in the sensor_temp_table array.
 * @window_sum: Sum of all elements in the array sensor_temp_table.
 * @temp_sensor: Pointer to temp_sensor of the domain.
 * @avg_sensor_temp_work: delayed_work structure for the stats computation.
 * @pm_notifier: pm notofier to start and stop the work function.
 */

struct stats_thermal {
	const char			*name;
	int				avg;
	int				avg_period;
	int				sensor_temp_table[AVERAGE_NUMBER];
	int				current_temp;
	bool				acc_is_valid;
	int				avg_enabled;
	uint				avg_num;
	int				trend;
	bool				trending_enabled;
	int				safe_temp_trend;
	int				stable_cnt;
	bool				is_stable;
	bool				accumulation_enabled;
	int				sample_index;
	int				window_sum;
	struct mutex			stats_mutex; /* Mutex for stats */
	struct thermal_dev		*temp_sensor;
	struct delayed_work		avg_sensor_temp_work;
	struct notifier_block		pm_notifier;
};

/**
 * struct thermal_dev  - Structure for each thermal device.
 * @name: The name of the device that is registering to the framework
 * @domain_name: The temperature domain that the thermal device represents
 * @dev: Device node
 * @dev_ops: The device specific operations for the sensor, governor and cooling
 *           agents.
 * @node: The list node of the
 * @current_temp: The current temperature reported for the specific domain
 *
 */
struct thermal_dev {
	const char	*name;
	const char	*domain_name;
	struct device	*dev;
	struct thermal_dev_ops *dev_ops;
	struct list_head cooling_actions;
	struct list_head node;
	int		current_temp;
	int		slope;
	int		constant;
	int		sen_id;
	int		stable_cnt;
	struct thermal_domain	*domain;
	struct stats_thermal	*stats;
	struct dentry *debug_dentry;
	/* for serializing sensor.ops manipulations and temp injection */
	struct mutex	mutex;
};

/**
 * Call the specific call back for a thermal device. In case some pointer
 * is missing, returns -EOPNOTSUPP, otherwise will return the result from
 * callback function.
 */
#define thermal_device_call(tdev, f, args...)				\
({									\
	int __err = -EOPNOTSUPP;					\
	if ((tdev) && (tdev)->dev_ops && (tdev)->dev_ops->f)		\
		__err = (tdev)->dev_ops->f((tdev) , ##args);		\
	__err;								\
})

/**
 * Call the specific call back for a set of thermal devices. It will call
 * until an error occurs or until all calls are done successfully. It will
 * continue even if any of the thermal devices is missing the callback.
 */
#define thermal_device_call_all(tdev_list, f, args...)			\
({									\
	struct thermal_dev *tdev;					\
	int ret = -ENODEV;						\
									\
	list_for_each_entry(tdev, (tdev_list), node) {			\
		ret = thermal_device_call(tdev, f , ##args);		\
		if (ret < 0)						\
			pr_debug("%s: failed to call " #f		\
				" on thermal device\n", __func__);	\
	}								\
	ret;								\
})

/**
 * Search a set of cooling actions for the specific reduction, based on
 * the required cooling level/priority.
 */
#define thermal_cooling_device_reduction_get(tdev, p)			\
({									\
	struct thermal_cooling_action *tcact;				\
	int ret = -ENODEV;						\
									\
	list_for_each_entry(tcact, &(tdev)->cooling_actions, node) {	\
		if (tcact->priority == (p))				\
			ret = tcact->reduction;				\
	}								\
	ret;								\
})

#ifdef CONFIG_THERMAL_FRAMEWORK
extern int thermal_insert_cooling_action(struct thermal_dev *tdev,
					 unsigned int priority,
					 unsigned int reduction);
extern int thermal_request_temp(struct thermal_dev *tdev);
extern int thermal_check_temp_stability(struct thermal_dev *tdev);
extern int thermal_enable_avg(const char *domain_name);
extern int thermal_enable_trend(const char *domain_name);
extern int thermal_lookup_temp(const char *domain_name);
extern int thermal_lookup_avg(const char *domain_name);
extern int thermal_lookup_trend(const char *domain_name);
extern int thermal_lookup_slope(const char *domain_name, const char *rel);
extern int thermal_lookup_offset(const char *domain_name, const char *rel);
extern int thermal_sensor_set_temp(struct thermal_dev *tdev);
extern int thermal_get_slope(struct thermal_dev *tdev, const char *rel);
extern int thermal_get_offset(struct thermal_dev *tdev, const char *rel);
extern int thermal_set_avg_period(struct thermal_dev *temp_sensor, int rate);
/* Registration and unregistration calls for the thermal devices */
extern int thermal_sensor_dev_register(struct thermal_dev *tdev);
extern void thermal_sensor_dev_unregister(struct thermal_dev *tdev);
extern int thermal_cooling_dev_register(struct thermal_dev *tdev);
extern void thermal_cooling_dev_unregister(struct thermal_dev *tdev);
extern int thermal_init_stats(struct thermal_dev *tdev, uint avg_period,
			uint avg_num, int safe_temp_trend);
extern int thermal_governor_dev_register(struct thermal_dev *tdev);
extern void thermal_governor_dev_unregister(struct thermal_dev *tdev);
extern int thermal_check_domain(const char *domain_name);
#else
static inline int thermal_insert_cooling_action(struct thermal_dev *tdev,
					 unsigned int priority,
					 unsigned int reduction)
{
	return 0;
}
static inline int thermal_request_temp(struct thermal_dev *tdev)
{
	return 0;
}
static inline int thermal_check_temp_stability(struct thermal_dev *tdev)
{
	return 0;
}
static inline int thermal_init_stats(struct thermal_dev *tdev, uint avg_period,
				     uint avg_num, int safe_temp_trend)
{
	return 0;
}
static inline int thermal_enable_avg(const char *domain_name)
{
	return 0;
}
static inline int thermal_enable_trend(const char *domain_name)
{
	return 0;
}
static inline int thermal_set_avg_period(struct thermal_dev *temp_sensor,
						int rate)
{
	return 0;
}
static inline int thermal_lookup_temp(const char *domain_name)
{
	return 0;
}
static inline int thermal_lookup_avg(const char *domain_name)
{
	return 0;
}
static inline int thermal_lookup_trend(const char *domain_name)
{
	return 0;
}
static
inline int thermal_lookup_slope(const char *domain_name, const char *rel)
{
	return 0;
}
static
inline int thermal_lookup_offset(const char *domain_name, const char *rel)
{
	return 0;
}
static inline int thermal_sensor_set_temp(struct thermal_dev *tdev)
{
	return 0;
}
static inline int thermal_get_slope(struct thermal_dev *tdev)
{
	return 0;
}
static inline int thermal_get_offset(struct thermal_dev *tdev)
{
	return 0;
}

/* Registration and unregistration calls for the thermal devices */
static inline int thermal_sensor_dev_register(struct thermal_dev *tdev)
{
	return 0;
}
static inline void thermal_sensor_dev_unregister(struct thermal_dev *tdev)
{
}
static inline int thermal_cooling_dev_register(struct thermal_dev *tdev)
{
	return 0;
}
static inline void thermal_cooling_dev_unregister(struct thermal_dev *tdev)
{
}
static inline int thermal_governor_dev_register(struct thermal_dev *tdev)
{
	return 0;
}
static inline void thermal_governor_dev_unregister(struct thermal_dev *tdev)
{
}
static inline int thermal_check_domain(const char *domain_name)
{
	return -ENODEV;
}
#endif

/* Specific to governors */
#ifdef CONFIG_CASE_TEMP_GOVERNOR
extern int case_subzone_number;
#else
#define case_subzone_number	-1
#endif

#endif /* __LINUX_THERMAL_FRAMEWORK_H__ */
