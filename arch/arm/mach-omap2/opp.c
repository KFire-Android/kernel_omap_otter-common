/*
 * OMAP SoC specific OPP wrapper function
 *
 * Copyright (C) 2009-2010 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *	Kevin Hilman
 * Copyright (C) 2010 Nokia Corporation.
 *      Eduardo Valentin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/opp.h>
#include <linux/clk.h>

#include <plat/omap_device.h>
#include <plat/clock.h>
#include <plat/dvfs.h>

#include "omap_opp_data.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "voltage.h"

static struct omap_opp_def *opp_table;
static u32 opp_table_size;

static void __init omap_opp_set_min_rate(struct omap_opp_def *opp_def)
{
	struct clk *clk;
	long round_rate;
	clk = omap_clk_get_by_name(opp_def->dev_info->clk_name);
	if (clk) {
		round_rate = clk_round_rate(clk, opp_def->freq);
		clk_set_rate(clk, round_rate);
		pr_info("%s: Missing opp info for hwmod %s\n",
			__func__, opp_def->dev_info->hwmod_name);
		pr_info("Forcing clock %s to minimum rate %ld\n",
			opp_def->dev_info->clk_name, round_rate);
	}
}

/**
 * omap_opp_register() - Initialize opp table as per the CPU type
 * @dev: device registering for OPP
 * @hwmod_name: hemod name of registering device
 *
 * Register the given device with the OPP/DVFS framework. Intended to
 * be called when omap_device is built.
 */
int omap_opp_register(struct device *dev, const char *hwmod_name)
{
	int i, r;
	struct clk *clk;
	long round_rate;
	struct omap_opp_def *opp_def = opp_table;
	u32 opp_def_size = opp_table_size;

	if (!opp_def || !opp_def_size) {
		pr_err("%s: invalid params!\n", __func__);
		return -EINVAL;
	}

	if (IS_ERR(dev)) {
		pr_err("%s: Unable to get dev pointer\n", __func__);
		return -EINVAL;
	}


	/* Lets now register with OPP library */
	for (i = 0; i < opp_def_size; i++, opp_def++) {
		if (!opp_def->default_available)
			continue;

		if (!opp_def->dev_info->hwmod_name) {
			WARN_ONCE(1, "%s: NULL name of omap_hwmod, failing [%d].\n",
				  __func__, i);
			return -EINVAL;
		}

		if (!strcmp(hwmod_name, opp_def->dev_info->hwmod_name)) {
			clk = omap_clk_get_by_name(opp_def->dev_info->clk_name);
			if (clk) {
				round_rate = clk_round_rate(clk, opp_def->freq);
				if (round_rate > 0) {
					opp_def->freq = round_rate;
				} else {
				pr_warn("%s: round_rate for clock %s failed\n",
					__func__, opp_def->dev_info->clk_name);
				continue; /* skip Bad OPP */
				}
			} else {
				pr_warn("%s: No clock by name %s found\n",
					__func__, opp_def->dev_info->clk_name);
				continue; /* skip Bad OPP */
			}
			r = opp_add(dev, opp_def->freq, opp_def->u_volt);
			if (r) {
				dev_err(dev,
					"%s: add OPP %ld failed for %s [%d] result=%d\n",
					__func__, opp_def->freq,
				       opp_def->dev_info->hwmod_name, i, r);
				continue;
			}

			r  = omap_dvfs_register_device(dev,
				       opp_def->dev_info->voltdm_name,
				       opp_def->dev_info->clk_name);
			if (r)
				dev_err(dev, "%s:%s:err dvfs register %d %d\n",
					__func__, opp_def->dev_info->hwmod_name,
					r, i);
		}
	}
	return 0;
}

/**
 * omap_init_opp_table() - Initialize opp table as per the CPU type
 * @opp_def:		opp default list for this silicon
 * @opp_def_size:	number of opp entries for this silicon
 *
 * Store the initial OPP table so that it can be used when devices
 * get registered with DVFS framework.
 */
int __init omap_init_opp_table(struct omap_opp_def *opp_def,
			u32 opp_def_size)
{
	if (!opp_def || !opp_def_size) {
		pr_err("%s: invalid params!\n", __func__);
		return -EINVAL;
	}

	if (!opp_table) {
		opp_table = opp_def;
		opp_table_size = opp_def_size;
	}

	return 0;
}

/**
 * set_device_opp() - set the default OPP for devices that are not
 * registered
 *
 * Sets the frequency for devices that are not available to lowest
 * possible one so that they will not prevent DVFS  transition.
 */
int __init set_device_opp(void)
{
	int i;
	struct omap_opp_def *opp_def = opp_table;
	u32 opp_def_size = opp_table_size;
	struct device_info *last_dev_info = NULL;

	if (!opp_def || !opp_def_size) {
		pr_err("%s: invalid params!\n", __func__);
		return -EINVAL;
	}

	/* Lets now register with OPP library */
	for (i = 0; i < opp_def_size; i++, opp_def++) {
		struct omap_hwmod *oh;

		if (!opp_def->default_available)
			continue;

		if (!opp_def->dev_info->hwmod_name) {
			WARN_ONCE(1, "%s: NULL name of omap_hwmod, " \
				  "failing [%d].\n", __func__, i);
			return -EINVAL;
		}

		if (last_dev_info != opp_def->dev_info) {
			oh = omap_hwmod_lookup(opp_def->dev_info->hwmod_name);
			if (!oh || !oh->od) {
				omap_opp_set_min_rate(opp_def);
				last_dev_info = opp_def->dev_info;
			}
		}
	}

	return 0;
}
