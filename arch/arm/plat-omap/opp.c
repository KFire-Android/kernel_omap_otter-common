/*
 * OMAP OPP Interface
 *
 * Copyright (C) 2009-2010 Texas Instruments Incorporated.
 *	Nishanth Menon
 *	Romit Dasgupta <romit@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/list.h>

#include <plat/opp.h>
#include <plat/omap_device.h>

/**
 * struct omap_opp - OMAP OPP description structure
 * @enabled:	true/false - marking this OPP as enabled/disabled
 * @rate:	Frequency in hertz
 * @u_volt:	Nominal voltage in microvolts corresponding to this OPP
 * @opp_id:	opp identifier (deprecated)
 *
 * This structure stores the OPP information for a given domain.
 */
struct omap_opp {
	struct list_head node;

	bool enabled;
	unsigned long rate;
	unsigned long u_volt;
	u8 opp_id;

	struct device_opp *dev_opp;  /* containing device_opp struct */
};

struct device_opp {
	struct list_head node;

	struct omap_hwmod *oh;
	struct device *dev;

	struct list_head opp_list;
	u32 opp_count;
	u32 enabled_opp_count;
};

static LIST_HEAD(dev_opp_list);

/**
 * find_device_opp() - find device_opp struct using device pointer
 * @dev: device pointer used to lookup device OPPs
 *
 * Search list of device OPPs for one containing matching device.
 *
 * Returns pointer to 'struct device_opp' if found, otherwise -ENODEV.
 */
static struct device_opp *find_device_opp(struct device *dev)
{
	struct device_opp *tmp_dev_opp, *dev_opp = ERR_PTR(-ENODEV);

	list_for_each_entry(tmp_dev_opp, &dev_opp_list, node) {
		if (tmp_dev_opp->dev == dev) {
			dev_opp = tmp_dev_opp;
			break;
		}
	}

	return dev_opp;
}

/**
 * opp_get_voltage() - Gets the voltage corresponding to an opp
 * @opp:	opp for which voltage has to be returned for
 *
 * Return voltage in micro volt corresponding to the opp, else
 * return 0
 */
unsigned long opp_get_voltage(const struct omap_opp *opp)
{
	if (unlikely(!opp || IS_ERR(opp)) || !opp->enabled) {
		pr_err("%s: Invalid parameters being passed\n", __func__);
		return 0;
	}

	return opp->u_volt;
}

/**
 * opp_get_freq() - Gets the frequency corresponding to an opp
 * @opp:	opp for which frequency has to be returned for
 *
 * Return frequency in hertz corresponding to the opp, else
 * return 0
 */
unsigned long opp_get_freq(const struct omap_opp *opp)
{
	if (unlikely(!opp || IS_ERR(opp)) || !opp->enabled) {
		pr_err("%s: Invalid parameters being passed\n", __func__);
		return 0;
	}

	return opp->rate;
}

/**
 * opp_find_by_opp_id - look up OPP by OPP ID (deprecated)
 * @opp_type:	OPP type where we want the look up to happen.
 * @opp_id:	OPP ID to search for
 *
 * Returns the struct omap_opp pointer corresponding to the given OPP
 * ID @opp_id, or returns NULL on error.
 */
struct omap_opp * __deprecated opp_find_by_opp_id(struct device *dev,
						  u8 opp_id)
{
	struct device_opp *dev_opp;
	struct omap_opp *temp_opp, *opp = ERR_PTR(-ENODEV);

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp))
		return opp;

	list_for_each_entry(temp_opp, &dev_opp->opp_list, node) {
		if (temp_opp->enabled && temp_opp->opp_id == opp_id) {
			opp = temp_opp;
			break;
		}
	}

	return opp;
}

/**
 * opp_get_opp_id() - Provide OPP ID corresponding to an OPP (deprecated)
 * @opp:	opp for which frequency has to be returned for
 *
 * Returns an OPP ID for the OPP required, if error, returns 0
 */
u8 __deprecated opp_get_opp_id(struct omap_opp *opp)
{
	if (unlikely(!opp || IS_ERR(opp)) || !opp->enabled) {
		pr_err("%s: Invalid parameter being passed\n", __func__);
		return 0;
	}

	return opp->opp_id;
}

/**
 * opp_get_opp_count() - Get number of opps enabled in the opp list
 * @opp_type:	OPP type we want to count
 *
 * This functions returns the number of opps if there are any OPPs enabled,
 * else returns corresponding error value.
 */
int opp_get_opp_count(struct device *dev)
{
	struct device_opp *dev_opp;

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp))
		return -ENODEV;

	return dev_opp->enabled_opp_count;
}

/**
 * opp_find_freq_exact() - search for an exact frequency
 * @opp_type:	OPP type we want to search in.
 * @freq:	frequency to search for
 * @enabled:	enabled/disabled OPP to search for
 *
 * Searches for exact match in the opp list and returns handle to the matching
 * opp if found, else returns ERR_PTR in case of error and should be handled
 * using IS_ERR.
 *
 * Note enabled is a modifier for the search. if enabled=true, then the match is
 * for exact matching frequency and is enabled. if false, the match is for exact
 * frequency which is disabled.
 */
struct omap_opp *opp_find_freq_exact(struct device *dev,
				     unsigned long freq, bool enabled)
{
	struct device_opp *dev_opp;
	struct omap_opp *temp_opp, *opp = ERR_PTR(-ENODEV);
	unsigned long req_freq = freq / 1000000;

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp))
		return opp;

	list_for_each_entry(temp_opp, &dev_opp->opp_list, node) {
		if (temp_opp->enabled) {
			unsigned long rate = temp_opp->rate / 1000000;

			if (rate == req_freq) {
				opp = temp_opp;
				break;
			}
		}
	}

	return opp;
}

/**
 * opp_find_freq_ceil() - Search for an rounded ceil freq
 * @opp_type:	OPP type where we want to search in
 * @freq:	Start frequency
 *
 * Search for the matching ceil *enabled* OPP from a starting freq
 * for a domain.
 *
 * Returns *opp and *freq is populated with the match, else
 * returns NULL opp if no match, else returns ERR_PTR in case of error.
 *
 * Example usages:
 *	* find match/next highest available frequency *
 *	freq = 350000;
 *	opp = opp_find_freq_ceil(OPP_MPU, &freq))
 *	if (IS_ERR(opp))
 *		pr_err("unable to find a higher frequency\n");
 *	else
 *		pr_info("match freq = %ld\n", freq);
 *
 *	* print all supported frequencies in ascending order *
 *	freq = 0; * Search for the lowest enabled frequency *
 *	while (!IS_ERR(opp = opp_find_freq_ceil(OPP_MPU, &freq)) {
 *		pr_info("freq = %ld\n", freq);
 *		freq++; * for next higher match *
 *	}
 */
struct omap_opp *opp_find_freq_ceil(struct device *dev, unsigned long *freq)
{
	struct device_opp *dev_opp;
	struct omap_opp *temp_opp, *opp = ERR_PTR(-ENODEV);
	unsigned long req_freq = *freq / 1000000;

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp))
		return opp;

	list_for_each_entry(temp_opp, &dev_opp->opp_list, node) {
		if (temp_opp->enabled) {
			unsigned long rate = temp_opp->rate / 1000000;

			if (rate >= req_freq) {
				opp = temp_opp;
				*freq = opp->rate;
				break;
			}
		}
	}

	return opp;
}

/**
 * opp_find_freq_floor() - Search for an rounded floor freq
 * @opp_type:	OPP type we want to search in
 * @freq:	Start frequency
 *
 * Search for the matching floor *enabled* OPP from a starting freq
 * for a domain.
 *
 * Returns *opp and *freq is populated with the next match, else
 * returns NULL opp if no match, else returns ERR_PTR in case of error.
 *
 * Example usages:
 *	* find match/next lowest available frequency
 *	freq = 350000;
 *	opp = opp_find_freq_floor(OPP_MPU, &freq)))
 *	if (IS_ERR(opp))
 *		pr_err ("unable to find a lower frequency\n");
 *	else
 *		pr_info("match freq = %ld\n", freq);
 *
 *	* print all supported frequencies in descending order *
 *	freq = ULONG_MAX; * search highest enabled frequency *
 *	while (!IS_ERR(opp = opp_find_freq_floor(OPP_MPU, &freq)) {
 *		pr_info("freq = %ld\n", freq);
 *		freq--; * for next lower match *
 *	}
 */
struct omap_opp *opp_find_freq_floor(struct device *dev, unsigned long *freq)
{
	struct device_opp *dev_opp;
	struct omap_opp *temp_opp, *opp = ERR_PTR(-ENODEV);
	unsigned long req_freq = *freq / 1000000;

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp))
		return opp;

	list_for_each_entry_reverse(temp_opp, &dev_opp->opp_list, node) {
		if (temp_opp->enabled) {
			unsigned long rate = temp_opp->rate / 1000000;

			if (rate <= req_freq) {
				opp = temp_opp;
				*freq = opp->rate;
				break;
			}
		}
	}

	return opp;
}

/* wrapper to reuse converting opp_def to opp struct */
static void omap_opp_populate(struct omap_opp *opp,
			      const struct omap_opp_def *opp_def)
{
	opp->rate = opp_def->freq;
	opp->enabled = opp_def->enabled;
	opp->u_volt = opp_def->u_volt;
}

/**
 * opp_add()  - Add an OPP table from a table definitions
 * @opp_def:	omap_opp_def to describe the OPP which we want to add.
 *
 * This function adds an opp definition to the opp list and returns status.
 */
int opp_add(const struct omap_opp_def *opp_def)
{
	struct omap_hwmod *oh;
	struct device *dev = NULL;
	struct device_opp *tmp_dev_opp, *dev_opp = NULL;
	struct omap_opp *opp, *new_opp;
	struct platform_device *pdev;
	struct list_head *head;
	int i;

	/* find the correct hwmod, and device */
	if (!opp_def->hwmod_name) {
		pr_err("%s: missing name of omap_hwmod, ignoring.\n", __func__);
		return -EINVAL;
	}
	oh = omap_hwmod_lookup(opp_def->hwmod_name);
	if (!oh || !oh->od) {
		pr_warn("%s: no hwmod for %s, cannot add OPPs.\n",
			__func__, opp_def->hwmod_name);
		return -EINVAL;
	}
	pdev = &oh->od->pdev;
	dev = &oh->od->pdev.dev;

	/* Check for existing list for 'dev' */
	list_for_each_entry(tmp_dev_opp, &dev_opp_list, node) {
		if (dev == tmp_dev_opp->dev) {
			dev_opp = tmp_dev_opp;
			break;
		}
	}

	if (!dev_opp) {
		/* Allocate a new device OPP table */
		dev_opp = kzalloc(sizeof(struct device_opp), GFP_KERNEL);
		if (WARN_ON(!dev_opp))
			return -ENOMEM;

		dev_opp->oh = oh;
		dev_opp->dev = &oh->od->pdev.dev;
		INIT_LIST_HEAD(&dev_opp->opp_list);

		list_add(&dev_opp->node, &dev_opp_list);
	}

	/* allocate new OPP node */
	new_opp = kzalloc(sizeof(struct omap_opp), GFP_KERNEL);
	if (WARN_ON(!new_opp))
		/* FIXME: free dev_opp ? */
		return -ENOMEM;
	omap_opp_populate(new_opp, opp_def);

	/* Insert new OPP in order of increasing frequency */
	head = &dev_opp->opp_list;
	list_for_each_entry_reverse(opp, &dev_opp->opp_list, node) {
		if (new_opp->rate >= opp->rate) {
			head = &opp->node;
			break;
		}
	}
	list_add(&new_opp->node, head);
	dev_opp->opp_count++;
	if (new_opp->enabled)
		dev_opp->enabled_opp_count++;

	/* renumber (deprecated) OPP IDs based on new order */
	i = 0;
	list_for_each_entry(opp, &dev_opp->opp_list, node)
		opp->opp_id = i++;

	return 0;
}

/**
 * opp_enable() - Enable a specific OPP
 * @opp:	Pointer to opp
 *
 * Enables a provided opp. If the operation is valid, this returns 0, else the
 * corresponding error value.
 *
 * OPP used here is from the the opp_is_valid/opp_has_freq or other search
 * functions
 */
int opp_enable(struct omap_opp *opp)
{

	if (unlikely(!opp || IS_ERR(opp))) {
		pr_err("%s: Invalid parameters being passed\n", __func__);
		return -EINVAL;
	}

	if (!opp->enabled && opp->dev_opp)
		opp->dev_opp->enabled_opp_count++;

	opp->enabled = true;

	return 0;
}

/**
 * opp_disable() - Disable a specific OPP
 * @opp:	Pointer to opp
 *
 * Disables a provided opp. If the operation is valid, this returns 0, else the
 * corresponding error value.
 *
 * OPP used here is from the the opp_is_valid/opp_has_freq or other search
 * functions
 */
int opp_disable(struct omap_opp *opp)
{
	if (unlikely(!opp || IS_ERR(opp))) {
		pr_err("%s: Invalid parameters being passed\n", __func__);
		return -EINVAL;
	}

	if (opp->enabled && opp->dev_opp)
		opp->dev_opp->enabled_opp_count--;

	opp->enabled = false;

	return 0;
}

/**
 * opp_init_cpufreq_table() - create a cpufreq table for a domain
 * @opp_type:	OPP type to initialize this list for
 * @table:	Cpufreq table returned back to caller
 *
 * Generate a cpufreq table for a provided domain - this assumes that the
 * opp list is already initialized and ready for usage
 */
void opp_init_cpufreq_table(struct device *dev,
			    struct cpufreq_frequency_table **table)
{
	struct device_opp *dev_opp;
	struct omap_opp *opp;
	struct cpufreq_frequency_table *freq_table;
	int i = 0;

	dev_opp = find_device_opp(dev);
	if (IS_ERR(dev_opp)) {
		WARN_ON(1);
		return;
	}

	freq_table = kzalloc(sizeof(struct cpufreq_frequency_table) *
			     (dev_opp->enabled_opp_count + 1), GFP_ATOMIC);
	if (!freq_table) {
		pr_warning("%s: failed to allocate frequency table\n",
			   __func__);
		return;
	}

	list_for_each_entry(opp, &dev_opp->opp_list, node) {
		if (opp->enabled) {
			freq_table[i].index = i;
			freq_table[i].frequency = opp->rate / 1000;
			i++;
		}
	}

	freq_table[i].index = i;
	freq_table[i].frequency = CPUFREQ_TABLE_END;

	*table = &freq_table[0];
}
