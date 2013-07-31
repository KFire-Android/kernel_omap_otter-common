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
#ifdef CONFIG_MACH_OMAP_4430_KC1
	int (*set_hot_event) (struct thermal_dev *temp_sensor,
			int hot_event);
#endif
	int (*set_temp_report_rate) (struct thermal_dev *, int rate);
	/* Cooling agent call backs */
	int (*cool_device) (struct thermal_dev *, int temp);
	/* Governor call backs */
	int (*process_temp) (struct thermal_dev *gov,
				struct list_head *cooling_list,
				struct thermal_dev *temp_sensor, int temp);
#ifdef CONFIG_MACH_OMAP_4430_KC1
	int (*process_hotspot_temp) (struct thermal_dev *tdev);
	int (*process_avg_temp) (struct thermal_dev *tdev);
	int (*process_zone) (struct thermal_dev *tdev);
#endif
#ifdef CONFIG_THERMAL_FRAMEWORK_DEBUG
	/* Debugging interface */
	int (*debug_report) (struct thermal_dev *, struct seq_file *s);
	int (*register_debug_entries) (struct thermal_dev *, struct dentry *d);
#endif
};

#ifdef CONFIG_MACH_OMAP_4430_KC1
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
#endif

/**
 * struct thermal_dev  - Structure for each thermal device.
 * @name: The name of the device that is registering to the framework
 * @domain_name: The temperature domain that the thermal device represents
 * @dev: Device node
 * @dev_ops: The device specific operations for the sensor, governor and cooling
 *           agents.
 * @node: The list node of the
 * @index: The index of the device created.
 * @current_temp: The current temperature reported for the specific domain
 *
 */
struct thermal_dev {
	const char	*name;
	const char	*domain_name;
	struct device	*dev;
	struct thermal_dev_ops *dev_ops;
#ifdef CONFIG_MACH_OMAP_4430_KC1
	struct list_head cooling_actions;
#endif
	struct list_head node;
	int 		current_temp;
	struct thermal_domain	*domain;
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

#ifdef CONFIG_MACH_OMAP_4430_KC1
/**
 * Call the specific call back for a given thermal devices "dev". It will call
 * the call back when it finds the matching entry from the list.
 */
#define thermal_device_call_dev(tdev_list, dev, f, args...)			\
({										\
	struct thermal_dev *tdev;						\
	int ret = -ENODEV;							\
										\
	list_for_each_entry(tdev, (tdev_list), node) {				\
		if (!(strcmp(tdev->name, dev))) {				\
			ret = thermal_device_call(tdev, f , ##args);		\
			if (ret < 0)						\
				pr_debug("%s: failed to call " #f		\
				" on thermal device\n", __func__);		\
			break;							\
		}								\
	}									\
	ret;									\
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
#endif

extern int thermal_request_temp(struct thermal_dev *tdev);
extern int thermal_lookup_temp(const char *domain_name);
extern int thermal_sensor_set_temp(struct thermal_dev *tdev);
#ifdef CONFIG_MACH_OMAP_4430_KC1
extern int thermal_sensor_get_hotspot_temp(struct thermal_dev *tdev);
extern int thermal_sensor_get_avg_temp(struct thermal_dev *tdev);
extern int thermal_sensor_get_zone(struct thermal_dev *tdev);
#endif

/* Registration and unregistration calls for the thermal devices */
extern int thermal_sensor_dev_register(struct thermal_dev *tdev);
extern void thermal_sensor_dev_unregister(struct thermal_dev *tdev);
extern int thermal_cooling_dev_register(struct thermal_dev *tdev);
extern void thermal_cooling_dev_unregister(struct thermal_dev *tdev);
extern int thermal_governor_dev_register(struct thermal_dev *tdev);
extern void thermal_governor_dev_unregister(struct thermal_dev *tdev);

#ifdef CONFIG_MACH_OMAP_4430_KC1
 /* Specific to governors */
#ifdef CONFIG_CASE_TEMP_GOVERNOR
extern int case_subzone_number;
#else
#define case_subzone_number	-1
#endif
#endif

#endif /* __LINUX_THERMAL_FRAMEWORK_H__ */
